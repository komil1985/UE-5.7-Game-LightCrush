// Copyright ASKD Games

#include "Components/kdWorldColorDriver.h"
#include "Data/kdColorTheme.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Kismet/GameplayStatics.h"


// ── MPC parameter name constants ─────────────────────────────────────────────
// These must match the parameter names you create inside your MPC asset.

const FName UkdWorldColorDriver::ParamName_WorldBlendAlpha = TEXT("WorldBlendAlpha");
const FName UkdWorldColorDriver::ParamName_CrushModeColor = TEXT("CrushModeColor");
const FName UkdWorldColorDriver::ParamName_LightWorldColor = TEXT("LightWorldColor");

// ─────────────────────────────────────────────────────────────────────────────

UkdWorldColorDriver::UkdWorldColorDriver()
{
    // Tick is expensive — keep it off until a blend is actually in progress
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UkdWorldColorDriver::BeginPlay()
{
    Super::BeginPlay();

    if (!ColorTheme)
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Warning,
            TEXT("WorldColorDriver: No ColorTheme assigned — component is inactive."));
#endif
        return;
    }

    // ── Acquire or create the PostProcessComponent ────────────────────────────
    // We look for an existing one first so we don't double-up if the player
    // Blueprint already has one placed manually.
    PostProcessComp = GetOwner()->FindComponentByClass<UPostProcessComponent>();

    if (!PostProcessComp)
    {
        PostProcessComp = NewObject<UPostProcessComponent>(
            GetOwner(),
            UPostProcessComponent::StaticClass(),
            TEXT("WorldColorPostProcess"));

        PostProcessComp->bAutoActivate = true;
        PostProcessComp->RegisterComponent();
    }

    // Unbound = affects the whole scene regardless of the player's position.
    // Priority 1000 overrides any world post-process volumes placed in levels.
    PostProcessComp->bUnbound = true;
    PostProcessComp->Priority = 1000.f;
    PostProcessComp->BlendWeight = 1.f;

    // ── Cache the MPC instance ────────────────────────────────────────────────
    if (ColorTheme->WorldColorMPC && GetWorld())
    {
        MPCInstance = GetWorld()->GetParameterCollectionInstance(ColorTheme->WorldColorMPC);
    }

    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("LightPPV"), Found);
    if (Found.Num() > 0)
    {
        CachedLightVolume = Cast<APostProcessVolume>(Found[0]);
        if (CachedLightVolume)
        {
            CachedLightVolume->BlendWeight = 1.f;
#if !UE_BUILD_SHIPPING
            UE_LOG(LogTemp, Log, TEXT("WorldColorDriver: Found LightPPV — %s"), *CachedLightVolume->GetName());
#endif
        }
    }

    Found.Reset();
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("ShadowPPV"), Found);
    if (Found.Num() > 0)
    {
        CachedShadowVolume = Cast<APostProcessVolume>(Found[0]);
        if (CachedShadowVolume)
        {
            CachedShadowVolume->BlendWeight = 0.f;  // starts off
#if !UE_BUILD_SHIPPING
            UE_LOG(LogTemp, Log, TEXT("WorldColorDriver: Found ShadowPPV — %s"), *CachedShadowVolume->GetName());
#endif
        }
    }

    // ── Register for State.CrushMode tag changes ──────────────────────────────
    // Same pattern used by AkdShadowPortal::BeginPlay.
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
    if (Player)
    {
        UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
        if (ASC)
        {
            ASC->RegisterGameplayTagEvent(
                FkdGameplayTags::Get().State_CrushMode,
                EGameplayTagEventType::NewOrRemoved)
                .AddUObject(this, &UkdWorldColorDriver::OnCrushModeTagChanged);

            // Sync immediately in case the player starts in CrushMode
            const bool bAlreadyCrushing =
                ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode);

            BlendAlpha = bAlreadyCrushing ? 1.f : 0.f;
        }
    }

    // Apply the starting profile with no animation
    ApplyProfileToPostProcess(BlendAlpha);
    UpdateMPC(BlendAlpha);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log,
        TEXT("WorldColorDriver: Initialized — starting alpha = %.2f."), BlendAlpha);
#endif
}

void UkdWorldColorDriver::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bBlending || !ColorTheme) return;

    const float BlendSpeed = (ColorTheme->BlendDuration > KINDA_SMALL_NUMBER)
        ? (1.f / ColorTheme->BlendDuration)
        : 100.f;

    BlendAlpha = FMath::Clamp(
        BlendAlpha + BlendDirection * BlendSpeed * DeltaTime, 0.f, 1.f);

    ApplyProfileToPostProcess(BlendAlpha);
    if (CachedShadowVolume) CachedShadowVolume->BlendWeight = BlendAlpha;
    if (CachedLightVolume)  CachedLightVolume->BlendWeight = 1.f - BlendAlpha;
    UpdateMPC(BlendAlpha);

    // ── Check whether we have reached the destination ─────────────────────────
    const bool bReachedDest =
        (BlendDirection > 0.f && BlendAlpha >= 1.f) ||
        (BlendDirection < 0.f && BlendAlpha <= 0.f);

    if (bReachedDest)
    {
        // Snap to exact destination to eliminate float drift
        BlendAlpha = (BlendDirection > 0.f) ? 1.f : 0.f;

        ApplyProfileToPostProcess(BlendAlpha);
        UpdateMPC(BlendAlpha);

        bBlending = false;
        SetComponentTickEnabled(false);

        BP_OnBlendFinished(BlendAlpha > 0.5f);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log,
            TEXT("WorldColorDriver: Blend finished — final alpha = %.2f."), BlendAlpha);
#endif
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private
// ─────────────────────────────────────────────────────────────────────────────

void UkdWorldColorDriver::OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    // NewCount > 0 → CrushMode just became active (entering shadow)
    // NewCount = 0 → CrushMode was just removed   (returning to light)
    StartBlend(NewCount > 0);
}

void UkdWorldColorDriver::StartBlend(bool bEnteringCrushMode)
{
    BlendDirection = bEnteringCrushMode ? 1.f : -1.f;
    bBlending = true;
    SetComponentTickEnabled(true);

    BP_OnBlendStarted(bEnteringCrushMode);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log,
        TEXT("WorldColorDriver: Blend started → %s  (alpha = %.2f)."),
        bEnteringCrushMode ? TEXT("Shadow") : TEXT("Light"),
        BlendAlpha);
#endif
}

void UkdWorldColorDriver::ApplyProfileToPostProcess(float Alpha) const
{
    if (!PostProcessComp || !ColorTheme) return;

    // Use full descriptive names — avoids single-letter collision with UE macro expansions
    const FkdPostProcessProfile& LightProf = ColorTheme->LightWorldProfile;
    const FkdPostProcessProfile& ShadowProf = ColorTheme->ShadowWorldProfile;

    FPostProcessSettings& PostProcSettings = PostProcessComp->Settings;

    // ── Scene Tint ───────────────────────────────────────────────────────────
    // Warm amber tint in light world; cool indigo tint in shadow world.
    PostProcSettings.bOverride_SceneColorTint = true;
    PostProcSettings.SceneColorTint = FLinearColor::LerpUsingHSV(
        LightProf.SceneColorTint, ShadowProf.SceneColorTint, Alpha);

    // ── White Balance — Temperature ───────────────────────────────────────────
    // UE 5.5+ color grading uses WhiteTemp/WhiteTint instead of the removed
    // ColorGradingShadows / ColorGradingMidtones / ColorGradingHighlights fields.
    // Temperature: 5500K (golden hour) → 9000K (cold moonlight).
    PostProcSettings.bOverride_WhiteTemp = true;
    PostProcSettings.WhiteTemp = FMath::Lerp(LightProf.WhiteTemp, ShadowProf.WhiteTemp, Alpha);

    // ── White Balance — Tint (green/magenta axis) ─────────────────────────────
    PostProcSettings.bOverride_WhiteTint = true;
    PostProcSettings.WhiteTint = FMath::Lerp(LightProf.WhiteTint, ShadowProf.WhiteTint, Alpha);

    // ── Saturation ───────────────────────────────────────────────────────────
    // UE5 stores saturation as FVector4 (R, G, B, Luma) where each channel
    // can be independently tuned.  We store a single float and expand uniformly.
    PostProcSettings.bOverride_ColorSaturation = true;
    const float LerpedSaturation = FMath::Lerp(LightProf.ColorSaturation, ShadowProf.ColorSaturation, Alpha);
    PostProcSettings.ColorSaturation = FVector4(LerpedSaturation, LerpedSaturation, LerpedSaturation, LerpedSaturation);

    // ── Vignette ─────────────────────────────────────────────────────────────
    PostProcSettings.bOverride_VignetteIntensity = true;
    PostProcSettings.VignetteIntensity = FMath::Lerp(
        LightProf.VignetteIntensity, ShadowProf.VignetteIntensity, Alpha);

    // ── Bloom ────────────────────────────────────────────────────────────────
    PostProcSettings.bOverride_BloomIntensity = true;
    PostProcSettings.BloomIntensity = FMath::Lerp(
        LightProf.BloomIntensity, ShadowProf.BloomIntensity, Alpha);
}

void UkdWorldColorDriver::UpdateMPC(float Alpha) const
{
    if (!MPCInstance || !ColorTheme) return;

    // WorldBlendAlpha — drives any material logic that reacts to world state
    MPCInstance->SetScalarParameterValue(ParamName_WorldBlendAlpha, Alpha);

    // Lerped tints for portals, pickup glows, shadow wall materials, etc.
    const FLinearColor CurrentCrush =
        FLinearColor::LerpUsingHSV(FLinearColor::Black, ColorTheme->DeepIndigo, Alpha);
    const FLinearColor CurrentLight =
        FLinearColor::LerpUsingHSV(ColorTheme->Sunstone, FLinearColor::White, Alpha);

    MPCInstance->SetVectorParameterValue(ParamName_CrushModeColor, CurrentCrush);
    MPCInstance->SetVectorParameterValue(ParamName_LightWorldColor, CurrentLight);
}


