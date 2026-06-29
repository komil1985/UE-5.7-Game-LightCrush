// Copyright ASKD Games

#include "Components/kdGameFeedbackComponent.h"
#include "Components/kdWorldColorDriver.h"
#include "Data/kdColorTheme.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Audio/kdAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

// ─────────────────────────────────────────────────────────────────────────────
// PP-material scalar param names — only what's still needed after Heliograph.
//
// Note: chromatic aberration is NOT driven through this custom PP material
// anymore.  It's now a built-in engine PP setting (SceneFringeIntensity)
// written by UkdWorldColorDriver — the only writer.  This material handles
// the vignette pulse layer only.
// ─────────────────────────────────────────────────────────────────────────────

static const FName PP_Vignette = TEXT("VignetteIntensity");

// ─────────────────────────────────────────────────────────────────────────────

UkdGameFeedbackComponent::UkdGameFeedbackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// =============================================================================
// BeginPlay
// =============================================================================
void UkdGameFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();

    AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
    if (!Player)
    {
        UE_LOG(LogTemp, Error,
            TEXT("GameFeedbackComponent: Owner is not AkdMyPlayer — disabled."));
        return;
    }

    CachedCamera = Player->Camera;
    CachedASC = Cast<UkdAbilitySystemComponent>(Player->GetAbilitySystemComponent());
    CachedWorldColorDriver = Player->FindComponentByClass<UkdWorldColorDriver>();

    if (!CachedASC)
    {
        UE_LOG(LogTemp, Error,
            TEXT("GameFeedbackComponent: No AbilitySystemComponent."));
    }

    // ── Character mesh DMI (rim + smear writes target this) ──────────────────
    if (USkeletalMeshComponent* Mesh = Player->GetMesh())
    {
        if (UMaterialInterface* SrcMat = Mesh->GetMaterial(RimMaterialSlotIndex))
        {
            CharMeshDMI = UMaterialInstanceDynamic::Create(SrcMat, this);
            Mesh->SetMaterial(RimMaterialSlotIndex, CharMeshDMI);
            CharMeshDMI->SetScalarParameterValue(RimIntensityParamName, 0.f);
            // Default rim colour is Lumen — pulled from the color theme via
            // the driver if it exists, otherwise sensible warm-white fallback.
            WriteMesh_RimColor(FLinearColor(0.93f, 0.84f, 0.61f, 1.f));
        }
    }

    // ── Custom PP material instance (vignette layer only) ────────────────────
    if (CrushPostProcessMaterial && CachedCamera)
    {
        PPInstance = UMaterialInstanceDynamic::Create(CrushPostProcessMaterial, this);
        if (PPInstance)
        {
            FWeightedBlendable Blendable;
            Blendable.Weight = 0.f;
            Blendable.Object = PPInstance;
            CachedCamera->PostProcessSettings.WeightedBlendables.Array.Add(Blendable);

            WritePP_Vignette(0.f);
            WritePP_BlendWeight(0.f);
        }
    }

    // ── GAS subscriptions ────────────────────────────────────────────────────
    if (CachedASC)
    {
        const FkdGameplayTags& Tags = FkdGameplayTags::Get();

        CachedASC->RegisterGameplayTagEvent(
            Tags.State_CrushMode, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &UkdGameFeedbackComponent::OnCrushModeTagChanged);

        CachedASC->RegisterGameplayTagEvent(
            Tags.State_InShadow, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &UkdGameFeedbackComponent::OnInShadowTagChanged);

        CachedASC->RegisterGameplayTagEvent(
            Tags.State_Exhausted, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &UkdGameFeedbackComponent::OnExhaustedTagChanged);

        CachedASC->GetGameplayAttributeValueChangeDelegate(
            UkdAttributeSet::GetShadowStaminaAttribute())
            .AddUObject(this, &UkdGameFeedbackComponent::OnStaminaChanged);

        // Seed CurrentStaminaFrac so the first frame is correct.
        CurrentStaminaFrac = ReadStaminaFraction();
    }
}

// =============================================================================
// TickComponent — gated; only on while transient effects are decaying.
// =============================================================================

void UkdGameFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ── Time-dilation freeze (uses real time) ────────────────────────────────
    if (FreezeStartRealTime >= 0.f)
    {
        const float Elapsed = GetWorld()->GetRealTimeSeconds() - FreezeStartRealTime;
        if (Elapsed >= FreezeDuration)
        {
            UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
            FreezeStartRealTime = -1.f;
        }
    }

    UpdateVignettePulse(DeltaTime);
    UpdateRim(DeltaTime);
    UpdateSmear(DeltaTime);

    if (!NeedsTick())
    {
        SetTickActive(false);

        // If the PP material is still showing zero values, hide its blend
        // weight entirely so it stops contributing GPU work.
        if (CurrentVignette < KINDA_SMALL_NUMBER
            && CurrentShadowVignette < KINDA_SMALL_NUMBER)
        {
            WritePP_BlendWeight(0.f);
        }
    }
}

UkdAudioSubsystem* UkdGameFeedbackComponent::GetAudioSubsystem() const
{
    const UWorld* World = GetWorld();
    if (!IsValid(World)) return nullptr;

    const UGameInstance* GI = World->GetGameInstance();
    if (!IsValid(GI)) return nullptr;

    return GI->GetSubsystem<UkdAudioSubsystem>();
}

// ---------------------------------------------------------------------------
// Called at the START of a crush transition (ability ActivateAbility)
// This is where enter/exit SWOOSH plays — matches your DA field names
// ---------------------------------------------------------------------------
void UkdGameFeedbackComponent::NotifyCrushTransitionStarted(bool bEntering)
{
    UE_LOG(LogTemp, Log, TEXT("GameFeedback: CrushTransitionStarted  enter=%d"), bEntering ? 1 : 0);

    // ---- POST-PROCESS burst (your existing WorldColorDriver call goes here) ----
    // e.g. WorldColorDriver->BeginCrushBlend(bEntering);

    // ---- AUDIO ----
    if (UkdAudioSubsystem* Audio = GetAudioSubsystem())
    {
        if (bEntering)
        {
            Audio->PlayCrushEnter();   // Plays DA_HeliographAudio.CrushEnterStart (Swoosh)
        }
        else
        {
            Audio->PlayCrushExit();    // Plays DA_HeliographAudio.CrushExitStart  (Swoosh)
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GameFeedback: AudioSubsystem not available at TransitionStarted — too early?"));
    }
}

// ---------------------------------------------------------------------------
// Called at the END of a crush transition (ability EndAbility / timer)
// ---------------------------------------------------------------------------
void UkdGameFeedbackComponent::NotifyCrushTransitionFinished(bool bNowInCrush)
{
    UE_LOG(LogTemp, Log, TEXT("GameFeedback: CrushTransitionFinished  nowCrush=%d"), bNowInCrush ? 1 : 0);

    // ---- POST-PROCESS settle (your existing call) ----

    // NOTE: No audio here by default.
    // CrushLand fires from the CMC landing callback (NotifyCrushLand), not here.
}

// ---------------------------------------------------------------------------
// Called when crush is blocked (Block.Crush tag active, stamina zero, etc.)
// ---------------------------------------------------------------------------
void UkdGameFeedbackComponent::NotifyCrushDenied()
{
    if (UkdAudioSubsystem* Audio = GetAudioSubsystem())
    {
        Audio->PlayCrushDenied(); // DA field is None right now — will no-op safely
    }
}

// ---------------------------------------------------------------------------
// Called from CrushStateComponent / CMC when landing after crush exit
// ---------------------------------------------------------------------------
void UkdGameFeedbackComponent::NotifyCrushLand()
{
    if (UkdAudioSubsystem* Audio = GetAudioSubsystem())
    {
        Audio->PlayCrushLand();   // Plays DA_HeliographAudio.CrushLand (Swoosh)
    }
}

// =============================================================================
// Public API
// =============================================================================

void UkdGameFeedbackComponent::OnDashPerformed(FVector DashDirection)
{
    PlayShake(DashShakeClass);
    PulseEdge(DashEdgePulseStrength);
    PulseChromatic(DashChromaticBurst);

    if (!DashDirection.IsNearlyZero())
    {
        LastDashDirection = DashDirection;
        CurrentSmear = DashSmearPeak;
        WriteMesh_Smear(CurrentSmear, LastDashDirection);
    }

    SetTickActive(true);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log,
        TEXT("GameFeedback: Dash performed.  Smear=%.2f  Dir=(0, %.2f, %.2f)  DMI=%s"),
        DashSmearPeak, LastDashDirection.Y, LastDashDirection.Z,
        CharMeshDMI ? TEXT("VALID") : TEXT("NULL"));
#endif
}

void UkdGameFeedbackComponent::OnCrushTransitionStarted(bool bToCrushMode)
{
    StartTimeFreeze();
    PlayShake(bToCrushMode ? CrushEnterShakeClass : CrushExitShakeClass);
    PulseEdge(bToCrushMode ? CrushEnterEdgePulseStrength : CrushExitEdgePulseStrength);
    PulseChromatic(bToCrushMode ? CrushEnterChromaticBurst : CrushExitChromaticBurst);

    SetTickActive(true);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log,
        TEXT("GameFeedback: CrushTransitionStarted  enter=%d"), bToCrushMode);
#endif
}

void UkdGameFeedbackComponent::OnCrushTransitionFinished(bool bNewCrushMode)
{
    PlayShake(CrushLandShakeClass);
    PulseEdge(CrushLandEdgePulseStrength);
    PulseChromatic(CrushLandChromaticBurst);
    SetTickActive(true);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log,
        TEXT("GameFeedback: CrushTransitionFinished  nowCrush=%d"), bNewCrushMode);
#endif
}

// =============================================================================
// Tag callbacks
// =============================================================================

void UkdGameFeedbackComponent::OnCrushModeTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
    bInCrushMode = (NewCount > 0);

    if (!bInCrushMode)
    {
        // Leaving crush mode — tear down any state owned by this component.
        TargetVignette = 0.f;
        TargetShadowVignette = 0.f;
        TargetRimIntensity = 0.f;
        VignettePhase = 0.f;
    }

    SetTickActive(true);
}

void UkdGameFeedbackComponent::OnInShadowTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
    const bool bInShadow = (NewCount > 0);

    PlayShake(bInShadow ? ShadowEntryShakeClass : ShadowExitShakeClass);

    if (bInShadow)
    {
        PulseEdge(ShadowEntryEdgePulseStrength);
        PulseChromatic(ShadowEntryChromaticBurst);
        SpawnShadowEntryNiagara();
    }

    // Rim flicks on entering shadow, fades out on leaving.
    TargetRimIntensity = bInShadow ? RimPeakIntensity : 0.f;
    TargetShadowVignette = bInShadow ? InShadowVignetteStrength : 0.f;

    if (bInShadow)
    {
        WritePP_BlendWeight(1.f);
    }

    SetTickActive(true);
}

void UkdGameFeedbackComponent::OnExhaustedTagChanged(const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
    // Tag-level signal is informational; the actual pulse amplitude is driven
    // by CurrentStaminaFrac in UpdateVignettePulse, fed by OnStaminaChanged.
    SetTickActive(true);
}

void UkdGameFeedbackComponent::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
    if (!CachedASC) return;

    const float Max = CachedASC->GetNumericAttribute(
        UkdAttributeSet::GetMaxShadowStaminaAttribute());

    CurrentStaminaFrac = (Max > 0.f)
        ? FMath::Clamp(Data.NewValue / Max, 0.f, 1.f)
        : 1.f;

    if (bInCrushMode && CurrentStaminaFrac < LowStaminaThreshold)
    {
        SetTickActive(true);
    }
}

// =============================================================================
// Per-tick updates
// =============================================================================

void UkdGameFeedbackComponent::UpdateVignettePulse(float DeltaTime)
{
    // ── Low-stamina danger pulse (only while in crush mode) ──────────────────
    if (bInCrushMode && CurrentStaminaFrac < LowStaminaThreshold)
    {
        WritePP_BlendWeight(1.f);

        // Pulse intensity ramps from 0 at threshold to MaxVignetteIntensity at 0.
        const float Danger = 1.f - (CurrentStaminaFrac / LowStaminaThreshold);
        VignettePhase += DeltaTime * VignettePulseFrequency * TWO_PI;
        if (VignettePhase > TWO_PI) VignettePhase -= TWO_PI;

        const float PulseWave = 0.5f + 0.5f * FMath::Sin(VignettePhase);
        TargetVignette = Danger * MaxVignetteIntensity * PulseWave;
    }

    CurrentVignette = FMath::FInterpTo(CurrentVignette, TargetVignette, DeltaTime, VignetteLerpSpeed);

    // ── Shadow-entry vignette burst (composes with the danger pulse) ─────────
    CurrentShadowVignette = FMath::FInterpTo(
        CurrentShadowVignette, TargetShadowVignette, DeltaTime, ShadowVignetteLerpSpeed);

    WritePP_Vignette(CurrentVignette + CurrentShadowVignette);
}

void UkdGameFeedbackComponent::UpdateRim(float DeltaTime)
{
    if (FMath::IsNearlyEqual(CurrentRimIntensity, TargetRimIntensity, 0.001f))
    {
        return;
    }

    CurrentRimIntensity = FMath::FInterpTo(
        CurrentRimIntensity, TargetRimIntensity, DeltaTime, RimLerpSpeed);

    WriteMesh_Rim(CurrentRimIntensity);
}

void UkdGameFeedbackComponent::UpdateSmear(float DeltaTime)
{
    if (CurrentSmear <= KINDA_SMALL_NUMBER) return;

    CurrentSmear = FMath::FInterpTo(CurrentSmear, 0.f, DeltaTime, SmearDecaySpeed);

    if (CurrentSmear < 0.005f)
    {
        CurrentSmear = 0.f;
        WriteMesh_Smear(0.f, FVector::ZeroVector);
    }
    else
    {
        WriteMesh_Smear(CurrentSmear, LastDashDirection);
    }
}

// =============================================================================
// Helpers
// =============================================================================

void UkdGameFeedbackComponent::PlayShake(TSubclassOf<UCameraShakeBase> ShakeClass) const
{
    if (!ShakeClass) return;

    const AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
    if (!Player) return;

    if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
    {
        PC->ClientStartCameraShake(ShakeClass, 1.f);
    }
}

void UkdGameFeedbackComponent::StartTimeFreeze()
{
    if (FreezeDilation >= 1.f - KINDA_SMALL_NUMBER) return;
    if (FreezeDuration <= KINDA_SMALL_NUMBER)        return;
    if (!GetWorld())                                  return;

    FreezeStartRealTime = GetWorld()->GetRealTimeSeconds();
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), FreezeDilation);
}

void UkdGameFeedbackComponent::PulseEdge(float Strength) const
{
    if (!CachedWorldColorDriver || Strength <= KINDA_SMALL_NUMBER) return;
    CachedWorldColorDriver->TriggerEdgePulse(Strength, EdgePulseDuration);
}

void UkdGameFeedbackComponent::PulseChromatic(float Intensity) const
{
    if (!CachedWorldColorDriver || Intensity <= KINDA_SMALL_NUMBER) return;
    CachedWorldColorDriver->TriggerChromaticBurst(Intensity, ChromaticBurstDuration);
}

// ── PP-material writes ───────────────────────────────────────────────────────

void UkdGameFeedbackComponent::WritePP_Vignette(float Value) const
{
    if (!PPInstance) return;
    PPInstance->SetScalarParameterValue(PP_Vignette, Value);
}

void UkdGameFeedbackComponent::WritePP_BlendWeight(float Weight) const
{
    if (!CachedCamera || !PPInstance) return;

    for (FWeightedBlendable& B : CachedCamera->PostProcessSettings.WeightedBlendables.Array)
    {
        if (B.Object == PPInstance)
        {
            B.Weight = Weight;
            return;
        }
    }
}

// ── Character mesh writes ────────────────────────────────────────────────────

void UkdGameFeedbackComponent::WriteMesh_Rim(float Intensity) const
{
    if (!CharMeshDMI) return;
    CharMeshDMI->SetScalarParameterValue(RimIntensityParamName, Intensity);
}

void UkdGameFeedbackComponent::WriteMesh_RimColor(const FLinearColor& Color) const
{
    if (!CharMeshDMI) return;
    CharMeshDMI->SetVectorParameterValue(RimColorParamName, Color);
}

void UkdGameFeedbackComponent::WriteMesh_Smear(float Strength, FVector PlanarDirection) const
{
    if (!CharMeshDMI)
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Warning,
            TEXT("GameFeedback: WriteMesh_Smear — CharMeshDMI is NULL. "
                "Check RimMaterialSlotIndex (%d) matches the slot with the smear material."),
            RimMaterialSlotIndex);
#endif
        return;
    }

    CharMeshDMI->SetScalarParameterValue(SmearStrengthParamName, Strength);

    // Convert the planar (Y, Z) direction into the format the smear material
    // expects.  X is always 0 in Shadow2D mode.
    const FLinearColor DirParam(0.f, -PlanarDirection.Y, -PlanarDirection.Z, 0.f);
    CharMeshDMI->SetVectorParameterValue(SmearDirectionParamName, DirParam);
}

// ── Niagara ──────────────────────────────────────────────────────────────────

void UkdGameFeedbackComponent::SpawnShadowEntryNiagara()
{
    if (!ShadowEntryNiagara) return;

    AActor* Owner = GetOwner();
    if (!Owner || !Owner->GetWorld()) return;

    const FVector SpawnLocation =
        Owner->GetActorLocation() + FVector(0.f, 0.f, ShadowEntryNiagaraZOffset);

    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        Owner->GetWorld(),
        ShadowEntryNiagara,
        SpawnLocation,
        FRotator::ZeroRotator,
        FVector(ShadowEntryNiagaraScale),
        /*bAutoDestroy*/   true,
        /*bAutoActivate*/  true,
        ENCPoolMethod::AutoRelease);
}

// ── Misc ─────────────────────────────────────────────────────────────────────

float UkdGameFeedbackComponent::ReadStaminaFraction() const
{
    if (!CachedASC) return 1.f;

    const float Max = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
    const float Cur = CachedASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());

    return (Max > 0.f) ? FMath::Clamp(Cur / Max, 0.f, 1.f) : 1.f;
}

void UkdGameFeedbackComponent::SetTickActive(bool bActive)
{
    PrimaryComponentTick.SetTickFunctionEnable(bActive);
}

bool UkdGameFeedbackComponent::NeedsTick() const
{
    return bInCrushMode
        || FreezeStartRealTime >= 0.f
        || CurrentSmear > KINDA_SMALL_NUMBER
        || FMath::Abs(CurrentVignette - TargetVignette) > 0.001f
        || FMath::Abs(CurrentShadowVignette - TargetShadowVignette) > 0.001f
        || FMath::Abs(CurrentRimIntensity - TargetRimIntensity) > 0.001f;
}
