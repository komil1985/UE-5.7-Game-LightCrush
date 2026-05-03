// Copyright ASKD Games

#include "Components/kdGameFeedbackComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"

static const FName PP_Chroma = TEXT("ChromaticAberration");
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
            TEXT("GameFeedbackComponent: Owner is not AkdMyPlayer! Disabled."));
        return;
    }

    CachedCamera = Player->Camera;
    CachedASC = Cast<UkdAbilitySystemComponent>(Player->GetAbilitySystemComponent());

    if (!CachedASC)
        UE_LOG(LogTemp, Error,
            TEXT("GameFeedbackComponent: No AbilitySystemComponent."));

    // ── CharMeshDMI ───────────────────────────────────────────────────────────
    if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
    {
        if (USkeletalMeshComponent* Mesh = Char->GetMesh())
        {
            if (UMaterialInterface* Mat = Mesh->GetMaterial(RimMaterialSlotIndex))
            {
                CharMeshDMI = UMaterialInstanceDynamic::Create(Mat, this);
                Mesh->SetMaterial(RimMaterialSlotIndex, CharMeshDMI);
                CharMeshDMI->SetScalarParameterValue(RimIntensityParamName, 0.f);
            }
        }
    }

    // ── PP material ───────────────────────────────────────────────────────────
    if (TryGetPPInstance())
    {
        WritePP_Chromatic(0.f);
        WritePP_Vignette(0.f);
        WritePP_BlendWeight(0.f);
    }

    // ── Initialise 3D camera grade (DOF off, full saturation) ─────────────────
    CurrentFStop = TargetFStop = 0.f;
    CurrentSensorWidth = TargetSensorWidth = 0.f;
    CurrentSaturation = TargetSaturation = 1.f;
    CurrentContrast = TargetContrast = 1.f;

    // ── GAS subscriptions ─────────────────────────────────────────────────────
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

        const float Max = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
        const float Cur = CachedASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());
        CurrentStaminaFrac = (Max > 0.f) ? FMath::Clamp(Cur / Max, 0.f, 1.f) : 1.f;
    }
}

// =============================================================================
// TickComponent
// =============================================================================

void UkdGameFeedbackComponent::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ── Time-dilation freeze ──────────────────────────────────────────────────
    // Use real time so duration is independent of the dilation we applied.
    if (FreezeStartRealTime >= 0.f)
    {
        const float RealElapsed = GetWorld()->GetRealTimeSeconds() - FreezeStartRealTime;
        if (RealElapsed >= FreezeDuration)
        {
            UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
            FreezeStartRealTime = -1.f;
        }
        // While frozen everything is near-still — still run the camera grade
        // lerp (uses DeltaTime × dilation which is tiny, but that's fine;
        // the grade transition looks smooth when time resumes).
    }

    // ── Chromatic decay ───────────────────────────────────────────────────────
    if (CurrentChromatic > KINDA_SMALL_NUMBER)
    {
        CurrentChromatic = FMath::Max(0.f, CurrentChromatic - ChromaticDecaySpeed * DeltaTime);
        WritePP_Chromatic(CurrentChromatic);
    }

    // ── Stamina vignette ──────────────────────────────────────────────────────
    if (bInCrushMode && CurrentStaminaFrac < LowStaminaThreshold)
    {
        const float Severity = 1.f - (CurrentStaminaFrac / LowStaminaThreshold);
        VignettePhase += VignettePulseFrequency * 2.f * PI * DeltaTime;
        TargetVignette = MaxVignetteIntensity * Severity * (0.5f + 0.5f * FMath::Sin(VignettePhase));
    }
    else
    {
        TargetVignette = 0.f;
        if (!bInCrushMode) VignettePhase = 0.f;
    }

    CurrentShadowVignette = FMath::FInterpTo(CurrentShadowVignette, TargetShadowVignette, DeltaTime, ShadowVignetteLerpSpeed);
    CurrentVignette = FMath::FInterpTo(CurrentVignette, TargetVignette, DeltaTime, VignetteLerpSpeed);
    WritePP_Vignette(FMath::Clamp(CurrentVignette + CurrentShadowVignette, 0.f, 1.f));

    // ── Rim glow ──────────────────────────────────────────────────────────────
    if (FMath::Abs(CurrentRimIntensity - TargetRimIntensity) > KINDA_SMALL_NUMBER)
    {
        CurrentRimIntensity = FMath::FInterpTo(CurrentRimIntensity, TargetRimIntensity, DeltaTime, RimLerpSpeed);
        WritePP_Rim(CurrentRimIntensity);
    }

    // ── Camera grade: DOF + saturation + contrast ─────────────────────────────
    // These lerp in Tick toward target values set by OnCrushModeTagChanged.
    // They write directly to Camera->PostProcessSettings — zero extra materials.
    {
        bool bGradeChanged = false;

        const float NewFStop = FMath::FInterpTo(CurrentFStop, TargetFStop, DeltaTime, CameraGradeLerpSpeed);
        const float NewSensor = FMath::FInterpTo(CurrentSensorWidth, TargetSensorWidth, DeltaTime, CameraGradeLerpSpeed);
        const float NewSat = FMath::FInterpTo(CurrentSaturation, TargetSaturation, DeltaTime, CameraGradeLerpSpeed);
        const float NewCon = FMath::FInterpTo(CurrentContrast, TargetContrast, DeltaTime, CameraGradeLerpSpeed);

        bGradeChanged = !FMath::IsNearlyEqual(NewFStop, CurrentFStop, 0.001f)
            || !FMath::IsNearlyEqual(NewSensor, CurrentSensorWidth, 0.1f)
            || !FMath::IsNearlyEqual(NewSat, CurrentSaturation, 0.001f)
            || !FMath::IsNearlyEqual(NewCon, CurrentContrast, 0.001f);

        CurrentFStop = NewFStop;
        CurrentSensorWidth = NewSensor;
        CurrentSaturation = NewSat;
        CurrentContrast = NewCon;

        if (bGradeChanged)
            WriteCameraGrade();
    }

    if (!NeedsTick())
        SetTickActive(false);
}

// =============================================================================
// WriteCameraGrade — single site for all direct camera PP writes
// =============================================================================

void UkdGameFeedbackComponent::WriteCameraGrade()
{
    if (!CachedCamera) return;

    // ── Depth of field ────────────────────────────────────────────────────────
    // We use Cinematic DOF (bOverride_DepthOfFieldFstop).
    // f-stop = 0 effectively disables DOF in UE without having to toggle overrides.
    if (bManageCrushDOF)
    {
        const bool bUseDOF = (CurrentFStop > 0.01f);

        CachedCamera->PostProcessSettings.bOverride_DepthOfFieldFstop = bUseDOF;
        CachedCamera->PostProcessSettings.bOverride_DepthOfFieldSensorWidth = bUseDOF;
        CachedCamera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = bUseDOF;

        if (bUseDOF)
        {
            CachedCamera->PostProcessSettings.DepthOfFieldFstop = FMath::Max(CurrentFStop, 0.5f);
            CachedCamera->PostProcessSettings.DepthOfFieldSensorWidth = CurrentSensorWidth;
            // Focus on the player: distance = spring arm length (player is at focus plane)
            // We store arm length indirectly by using CachedCamera's parent hierarchy,
            // but the simplest safe value is a fixed typical crush arm length.
            // If the user has a different arm length they can override via BP.
            CachedCamera->PostProcessSettings.DepthOfFieldFocalDistance = 2200.f;
        }
    }

    // ── Color grading — Saturation ────────────────────────────────────────────
    // FVector4 (R, G, B, A) where A is the global multiplier.
    // We write only to A and leave RGB at 1 so no hue shift occurs.
    CachedCamera->PostProcessSettings.bOverride_ColorSaturation = true;
    CachedCamera->PostProcessSettings.ColorSaturation =
        FVector4(1.f, 1.f, 1.f, CurrentSaturation);

    // ── Color grading — Contrast ──────────────────────────────────────────────
    CachedCamera->PostProcessSettings.bOverride_ColorContrast = true;
    CachedCamera->PostProcessSettings.ColorContrast =
        FVector4(1.f, 1.f, 1.f, CurrentContrast);
}

// =============================================================================
// Public API
// =============================================================================

void UkdGameFeedbackComponent::OnDashPerformed()
{
    PlayShake(DashShakeClass);
    CurrentChromatic = DashChromaticPeak;
    WritePP_Chromatic(CurrentChromatic);
    SetTickActive(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnCrushTransitionStarted
// ─────────────────────────────────────────────────────────────────────────────

void UkdGameFeedbackComponent::OnCrushTransitionStarted(bool bToCrushMode)
{
#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("GFC: CrushTransitionStarted (enter=%d)"), bToCrushMode);
#endif

    if (GetWorld())
        GetWorld()->GetTimerManager().ClearTimer(PPHideTimerHandle);

    // ── Time-dilation freeze ──────────────────────────────────────────────────
    if (FreezeDilation < 1.f - KINDA_SMALL_NUMBER && FreezeDuration > KINDA_SMALL_NUMBER)
    {
        FreezeStartRealTime = GetWorld()->GetRealTimeSeconds();
        UGameplayStatics::SetGlobalTimeDilation(GetWorld(), FreezeDilation);
    }

    WritePP_BlendWeight(1.f);
    CurrentChromatic = bToCrushMode ? CrushEnterChromaticPeak : CrushExitChromaticPeak;
    WritePP_Chromatic(CurrentChromatic);
    PlayShake(bToCrushMode ? CrushEnterShakeClass : CrushExitShakeClass);
    SetTickActive(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnCrushTransitionFinished
// ─────────────────────────────────────────────────────────────────────────────

void UkdGameFeedbackComponent::OnCrushTransitionFinished(bool bNewCrushMode)
{
#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("GFC: CrushTransitionFinished (nowCrush=%d)"), bNewCrushMode);
#endif

    CurrentChromatic = FMath::Max(CurrentChromatic, CrushLandChromaticPeak);
    WritePP_Chromatic(CurrentChromatic);
    PlayShake(CrushLandShakeClass);
    SetTickActive(true);

    if (!bNewCrushMode)
        SchedulePPHide();
}

// ─────────────────────────────────────────────────────────────────────────────
// SchedulePPHide / HidePP
// ─────────────────────────────────────────────────────────────────────────────

void UkdGameFeedbackComponent::SchedulePPHide()
{
    if (!GetWorld()) return;

    const float HideDelay = (ChromaticDecaySpeed > KINDA_SMALL_NUMBER)
        ? (CrushLandChromaticPeak / ChromaticDecaySpeed) + 0.05f
        : 0.5f;

    GetWorld()->GetTimerManager().SetTimer(
        PPHideTimerHandle, this,
        &UkdGameFeedbackComponent::HidePP,
        HideDelay, false);
}

void UkdGameFeedbackComponent::HidePP()
{
    CurrentChromatic = CurrentVignette = TargetVignette = 0.f;
    CurrentShadowVignette = TargetShadowVignette = 0.f;
    CurrentRimIntensity = TargetRimIntensity = 0.f;
    VignettePhase = 0.f;

    WritePP_Chromatic(0.f);
    WritePP_Vignette(0.f);
    WritePP_Rim(0.f);
    WritePP_BlendWeight(0.f);
}

// =============================================================================
// Tag callbacks
// =============================================================================

void UkdGameFeedbackComponent::OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    bInCrushMode = (NewCount > 0);

    if (bInCrushMode)
    {
        // PP layer visible; start camera grade lerp toward 2D targets.
        WritePP_BlendWeight(1.f);

        if (bManageCrushDOF)
        {
            TargetFStop = Crush2DFStop;
            TargetSensorWidth = Crush2DSensorWidth;
        }
        TargetSaturation = Crush2DSaturation;
        TargetContrast = Crush2DContrast;
    }
    else
    {
        // Exiting crush: lerp grade back to 3D defaults.
        TargetFStop = 0.f;
        TargetSensorWidth = 0.f;
        TargetSaturation = 1.f;
        TargetContrast = 1.f;

        // Do NOT kill PP blend weight here — SchedulePPHide handles that
        // after the landing chromatic echo decays.
        TargetVignette = TargetShadowVignette = TargetRimIntensity = 0.f;
        VignettePhase = 0.f;
    }

    SetTickActive(true);
}

void UkdGameFeedbackComponent::OnInShadowTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        PlayShake(ShadowEntryShakeClass);
        CurrentChromatic = ShadowEntryChromaticPeak;
        WritePP_Chromatic(CurrentChromatic);
    }
    else
    {
        PlayShake(ShadowExitShakeClass);
    }

    TargetRimIntensity = (NewCount > 0) ? RimPeakIntensity : 0.f;
    TargetShadowVignette = (NewCount > 0) ? InShadowVignetteStrength : 0.f;
    SetTickActive(true);
}

void UkdGameFeedbackComponent::OnExhaustedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        PlayShake(StaminaEmptyShakeClass);
        SetTickActive(true);
    }
}

// =============================================================================
// Attribute callback
// =============================================================================

void UkdGameFeedbackComponent::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
    if (!CachedASC) return;

    const float Max = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
    CurrentStaminaFrac = (Max > 0.f) ? FMath::Clamp(Data.NewValue / Max, 0.f, 1.f) : 1.f;

    if (bInCrushMode && CurrentStaminaFrac < LowStaminaThreshold)
        SetTickActive(true);
}

// =============================================================================
// Helpers
// =============================================================================

void UkdGameFeedbackComponent::PlayShake(TSubclassOf<UCameraShakeBase> ShakeClass)
{
    if (!ShakeClass) return;

    AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
    if (!Player) return;

    if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
        PC->ClientStartCameraShake(ShakeClass, 1.f);
}

bool UkdGameFeedbackComponent::TryGetPPInstance()
{
    if (PPInstance) return true;
    if (!CrushPostProcessMaterial || !CachedCamera) return false;

    PPInstance = UMaterialInstanceDynamic::Create(CrushPostProcessMaterial, this);
    if (!PPInstance) return false;

    FWeightedBlendable B;
    B.Weight = 0.f;
    B.Object = PPInstance;
    CachedCamera->PostProcessSettings.WeightedBlendables.Array.Add(B);
    return true;
}

void UkdGameFeedbackComponent::WritePP_Chromatic(float Value)
{
    if (!PPInstance && !TryGetPPInstance()) return;
    PPInstance->SetScalarParameterValue(PP_Chroma, Value);
}

void UkdGameFeedbackComponent::WritePP_Vignette(float Value)
{
    if (!PPInstance && !TryGetPPInstance()) return;
    PPInstance->SetScalarParameterValue(PP_Vignette, Value);
}

void UkdGameFeedbackComponent::WritePP_BlendWeight(float Weight)
{
    if (!CachedCamera) return;
    for (FWeightedBlendable& B : CachedCamera->PostProcessSettings.WeightedBlendables.Array)
    {
        if (B.Object == PPInstance) { B.Weight = Weight; return; }
    }
}

void UkdGameFeedbackComponent::WritePP_Rim(float Value)
{
    if (!CharMeshDMI) return;
    CharMeshDMI->SetScalarParameterValue(RimIntensityParamName, Value);
}

void UkdGameFeedbackComponent::SetTickActive(bool bActive)
{
    PrimaryComponentTick.SetTickFunctionEnable(bActive);
}

bool UkdGameFeedbackComponent::NeedsTick() const
{
    return bInCrushMode
        || FreezeStartRealTime >= 0.f
        || CurrentChromatic > KINDA_SMALL_NUMBER
        || FMath::Abs(CurrentVignette - TargetVignette) > KINDA_SMALL_NUMBER
        || FMath::Abs(CurrentShadowVignette - TargetShadowVignette) > KINDA_SMALL_NUMBER
        || FMath::Abs(CurrentRimIntensity - TargetRimIntensity) > KINDA_SMALL_NUMBER
        || FMath::Abs(CurrentFStop - TargetFStop) > 0.01f
        || FMath::Abs(CurrentSaturation - TargetSaturation) > 0.001f
        || FMath::Abs(CurrentContrast - TargetContrast) > 0.001f;
}
