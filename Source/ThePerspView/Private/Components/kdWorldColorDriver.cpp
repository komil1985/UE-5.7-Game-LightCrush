// Copyright ASKD Games

#include "Components/kdWorldColorDriver.h"
#include "Data/kdColorTheme.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Components/PostProcessComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Kismet/GameplayStatics.h"

// ─────────────────────────────────────────────────────────────────────────────
// MPC parameter names — must match the asset created in editor.
// Renamed for the Heliograph contract:
//   WorldBlendAlpha   (scalar)  — 0 light, 1 crush
//   EdgePulseAlpha    (scalar)  — transient 0..1 burst
//   LumenColor        (vector)  — outline trace
//   SolarColor        (vector)  — exposed silhouette
//   IndigoFieldColor  (vector)  — crush background
// ─────────────────────────────────────────────────────────────────────────────

const FName UkdWorldColorDriver::ParamName_WorldBlendAlpha = TEXT("WorldBlendAlpha");
const FName UkdWorldColorDriver::ParamName_EdgePulseAlpha = TEXT("EdgePulseAlpha");
const FName UkdWorldColorDriver::ParamName_ShadowTintAlpha = TEXT("ShadowTintAlpha");
const FName UkdWorldColorDriver::ParamName_LumenColor = TEXT("LumenColor");
const FName UkdWorldColorDriver::ParamName_SolarColor = TEXT("SolarColor");
const FName UkdWorldColorDriver::ParamName_IndigoFieldColor = TEXT("IndigoFieldColor");

// ─────────────────────────────────────────────────────────────────────────────

UkdWorldColorDriver::UkdWorldColorDriver()
{
    // Tick is gated — only on while a blend or edge pulse is in flight.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void UkdWorldColorDriver::BeginPlay()
{
    Super::BeginPlay();

    // ── Find or create the unbound PostProcessComponent owned by the player ──
    if (ColorTheme)
    {
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

        PostProcessComp->bUnbound = true;
        PostProcessComp->Priority = 1000.f;
        PostProcessComp->BlendWeight = 1.f;

        // Resolve MPC instance once — used by every UpdateMPC call.
        if (ColorTheme->WorldColorMPC && GetWorld())
        {
            MPCInstance = GetWorld()->GetParameterCollectionInstance(ColorTheme->WorldColorMPC);
        }
    }

    // ── Optional level-placed PP volumes — picked up by tag ──────────────────
    TArray<AActor*> Found;

    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("LightPPV"), Found);
    if (Found.Num() > 0)
    {
        CachedLightVolume = Cast<APostProcessVolume>(Found[0]);
        if (CachedLightVolume) CachedLightVolume->BlendWeight = 1.f;
    }

    Found.Reset();
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("ShadowPPV"), Found);
    if (Found.Num() > 0)
    {
        CachedShadowVolume = Cast<APostProcessVolume>(Found[0]);
        if (CachedShadowVolume) CachedShadowVolume->BlendWeight = 0.f;
    }

    // ── Register for CrushMode tag changes ───────────────────────────────────
    if (AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent())
        {
            ASC->RegisterGameplayTagEvent(
                FkdGameplayTags::Get().State_CrushMode,
                EGameplayTagEventType::NewOrRemoved)
                .AddUObject(this, &UkdWorldColorDriver::OnCrushModeTagChanged);

            // Shadow tint follows State.InShadow — the tag only exists while
            // the player is in shadow DURING Crush Mode, and is removed by
            // both the toggle ability and CrushStateComponent on crush exit,
            // so this single registration covers "revert on leaving shadow
            // OR leaving 2D mode" with no extra bookkeeping.
            ASC->RegisterGameplayTagEvent(
                FkdGameplayTags::Get().State_InShadow,
                EGameplayTagEventType::NewOrRemoved)
                .AddUObject(this, &UkdWorldColorDriver::OnInShadowTagChanged);

            // Seed from current state — covers respawn / level load mid-shadow.
            ShadowTintTarget = ASC->HasMatchingGameplayTag(
                FkdGameplayTags::Get().State_InShadow) ? 1.f : 0.f;
            ShadowTintAlpha = ShadowTintTarget;

            // Initialise to current tag state — covers respawn / level load
            BlendAlpha = ASC->HasMatchingGameplayTag(
                FkdGameplayTags::Get().State_CrushMode) ? 1.f : 0.f;
        }
    }

    // Apply the starting world look immediately so the first frame is correct.
    if (CachedShadowVolume) CachedShadowVolume->BlendWeight = BlendAlpha;
    if (CachedLightVolume)  CachedLightVolume->BlendWeight = 1.f - BlendAlpha;
    ApplyProfileToPostProcess(BlendAlpha);
    UpdateMPC(BlendAlpha);
    WriteEdgePulseAlpha(0.f);
    WriteShadowTintAlpha(ShadowTintAlpha);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — drives the steady-state blend and the transient edge pulse.
// ─────────────────────────────────────────────────────────────────────────────

void UkdWorldColorDriver::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ── Steady-state blend ───────────────────────────────────────────────────
    if (bBlending)
    {
        const float BlendSpeed = (ColorTheme && ColorTheme->BlendDuration > KINDA_SMALL_NUMBER)
            ? (1.f / ColorTheme->BlendDuration)
            : (1.f / 0.35f);

        BlendAlpha = FMath::Clamp(
            BlendAlpha + BlendDirection * BlendSpeed * DeltaTime, 0.f, 1.f);

        if (CachedShadowVolume) CachedShadowVolume->BlendWeight = BlendAlpha;
        if (CachedLightVolume)  CachedLightVolume->BlendWeight = 1.f - BlendAlpha;

        ApplyProfileToPostProcess(BlendAlpha);
        UpdateMPC(BlendAlpha);

        const bool bReachedDest =
            (BlendDirection > 0.f && BlendAlpha >= 1.f) ||
            (BlendDirection < 0.f && BlendAlpha <= 0.f);

        if (bReachedDest)
        {
            // Hard-snap to exact target — eliminates floating-point residual.
            BlendAlpha = (BlendDirection > 0.f) ? 1.f : 0.f;

            if (CachedShadowVolume) CachedShadowVolume->BlendWeight = BlendAlpha;
            if (CachedLightVolume)  CachedLightVolume->BlendWeight = 1.f - BlendAlpha;
            ApplyProfileToPostProcess(BlendAlpha);
            UpdateMPC(BlendAlpha);

            bBlending = false;
            BP_OnBlendFinished(BlendAlpha > 0.5f);
        }
    }

    // ── Transient edge pulse decay ───────────────────────────────────────────
    if (EdgePulseCurrent > KINDA_SMALL_NUMBER)
    {
        EdgePulseCurrent = FMath::Max(0.f, EdgePulseCurrent - EdgePulseDecayRate * DeltaTime);
        WriteEdgePulseAlpha(EdgePulseCurrent);
    }

    // ── Transient chromatic burst decay ──────────────────────────────────────
    // If a blend just ran, ApplyProfileToPostProcess already composed
    // baseline + burst into SceneFringeIntensity, so we'd double-write.  Only
    // refresh from here when no blend is active — that's the case where the
    // baseline isn't changing but the burst is still decaying toward zero.
    if (ChromaticBurstCurrent > KINDA_SMALL_NUMBER)
    {
        ChromaticBurstCurrent = FMath::Max(
            0.f, ChromaticBurstCurrent - ChromaticBurstDecayRate * DeltaTime);

        if (!bBlending)
        {
            WriteChromaticAberration();
        }
    }

    // ── Shadow tint fade (player in-shadow, Crush Mode only) ─────────────────
    if (!FMath::IsNearlyEqual(ShadowTintAlpha, ShadowTintTarget))
    {
        if (ShadowTintFadeDuration > KINDA_SMALL_NUMBER)
        {
            ShadowTintAlpha = FMath::FInterpConstantTo(
                ShadowTintAlpha, ShadowTintTarget,
                DeltaTime, 1.f / ShadowTintFadeDuration);
        }
        else
        {
            ShadowTintAlpha = ShadowTintTarget;   // instant snap
        }
        WriteShadowTintAlpha(ShadowTintAlpha);
    }

    // ── Gate tick when nothing left to drive ─────────────────────────────────
    if (!NeedsTick())
    {
        SetComponentTickEnabled(false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void UkdWorldColorDriver::TriggerEdgePulse(float Strength, float Duration)
{
    EdgePulseCurrent = FMath::Clamp(Strength, 0.f, 1.f);
    EdgePulseDecayRate = (Duration > KINDA_SMALL_NUMBER) ? (EdgePulseCurrent / Duration) : 0.f;

    WriteEdgePulseAlpha(EdgePulseCurrent);
    SetComponentTickEnabled(true);
}

void UkdWorldColorDriver::TriggerChromaticBurst(float Intensity, float Duration)
{
    // SceneFringeIntensity is unbounded in UE5 but values above ~5 read as a
    // glitch.  Clamp here so a runaway gameplay event can't produce a dizzy peak.
    ChromaticBurstCurrent = FMath::Clamp(Intensity, 0.f, 5.f);
    ChromaticBurstDecayRate = (Duration > KINDA_SMALL_NUMBER)
        ? (ChromaticBurstCurrent / Duration)
        : 0.f;

    WriteChromaticAberration();
    SetComponentTickEnabled(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private
// ─────────────────────────────────────────────────────────────────────────────

void UkdWorldColorDriver::OnCrushModeTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
    // Leaving Crush Mode always reverts the shadow tint, even if the
    // State.InShadow removal event were ever missed.
    if (NewCount <= 0)
    {
        ShadowTintTarget = 0.f;
    }
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
        bEnteringCrushMode ? TEXT("Crush") : TEXT("Light"),
        BlendAlpha);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyProfileToPostProcess
//
// Lerps every field on the FkdPostProcessProfile struct from the Light profile
// to the Crush profile based on Alpha and writes the result to the unbound
// post-process component.  Each PostProcessSettings field has its corresponding
// bOverride_ flag flipped on so the lerped value actually takes effect.
// ─────────────────────────────────────────────────────────────────────────────

void UkdWorldColorDriver::ApplyProfileToPostProcess(float Alpha) const
{
    if (!PostProcessComp || !ColorTheme) return;

    const FkdPostProcessProfile& LightProf = ColorTheme->LightWorldProfile;
    const FkdPostProcessProfile& CrushProf = ColorTheme->CrushWorldProfile;

    FPostProcessSettings& Settings = PostProcessComp->Settings;

    // ── Scene tint ───────────────────────────────────────────────────────────
    Settings.bOverride_SceneColorTint = true;
    Settings.SceneColorTint = FLinearColor::LerpUsingHSV(
        LightProf.SceneColorTint, CrushProf.SceneColorTint, Alpha);

    // ── White balance ────────────────────────────────────────────────────────
    Settings.bOverride_WhiteTemp = true;
    Settings.WhiteTemp = FMath::Lerp(LightProf.WhiteTemp, CrushProf.WhiteTemp, Alpha);

    Settings.bOverride_WhiteTint = true;
    Settings.WhiteTint = FMath::Lerp(LightProf.WhiteTint, CrushProf.WhiteTint, Alpha);

    // ── Saturation ───────────────────────────────────────────────────────────
    // ColorSaturation is FVector4(R, G, B, Luma).  Writing the same value to
    // all four channels produces a clean uniform desaturation with no hue shift.
    const float Sat = FMath::Lerp(LightProf.ColorSaturation, CrushProf.ColorSaturation, Alpha);
    Settings.bOverride_ColorSaturation = true;
    Settings.ColorSaturation = FVector4(Sat, Sat, Sat, Sat);

    // ── Vignette / Bloom ─────────────────────────────────────────────────────
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = FMath::Lerp(
        LightProf.VignetteIntensity, CrushProf.VignetteIntensity, Alpha);

    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = FMath::Lerp(
        LightProf.BloomIntensity, CrushProf.BloomIntensity, Alpha);

    // ── Depth of Field ───────────────────────────────────────────────────────
    // Centralised here; the transition component no longer touches DOF.
    Settings.bOverride_DepthOfFieldFstop = true;
    Settings.DepthOfFieldFstop = FMath::Lerp(
        LightProf.DepthOfFieldFstop, CrushProf.DepthOfFieldFstop, Alpha);

    Settings.bOverride_DepthOfFieldSensorWidth = true;
    Settings.DepthOfFieldSensorWidth = FMath::Lerp(
        LightProf.DepthOfFieldSensorWidth, CrushProf.DepthOfFieldSensorWidth, Alpha);

    // ── Chromatic Aberration ─────────────────────────────────────────────────
    // Steady-state baseline lerped between profiles, with any active transient
    // burst added on top.  Single writer on this field — never write directly
    // from a feedback component; route bursts through TriggerChromaticBurst().
    const float ChromaticBaseline = FMath::Lerp(
        LightProf.ChromaticAberrationIntensity,
        CrushProf.ChromaticAberrationIntensity, Alpha);
    Settings.bOverride_SceneFringeIntensity = true;
    Settings.SceneFringeIntensity = ChromaticBaseline + ChromaticBurstCurrent;
}

// ─────────────────────────────────────────────────────────────────────────────
// UpdateMPC
//
// Writes the Heliograph contract to the material parameter collection.  Any
// material in the project can read these scalars/vectors to react to world
// state — edge-trace materials, exposed-silhouette stencils, portal glows.
// ─────────────────────────────────────────────────────────────────────────────

void UkdWorldColorDriver::UpdateMPC(float Alpha) const
{
    if (!MPCInstance || !ColorTheme) return;

    MPCInstance->SetScalarParameterValue(ParamName_WorldBlendAlpha, Alpha);

    // Vectors lerp toward their crush-mode values as Alpha increases.
    // Light-world side stays neutral (white-ish) so materials reading these in
    // light mode have a sensible fallback rather than crush-tinted defaults.
    const FLinearColor LerpedLumen =
        FLinearColor::LerpUsingHSV(FLinearColor::White, ColorTheme->Lumen, Alpha);
    const FLinearColor LerpedSolar =
        FLinearColor::LerpUsingHSV(FLinearColor::White, ColorTheme->SolarWhite, Alpha);
    const FLinearColor LerpedField =
        FLinearColor::LerpUsingHSV(ColorTheme->Atmosphere, ColorTheme->IndigoField, Alpha);

    MPCInstance->SetVectorParameterValue(ParamName_LumenColor, LerpedLumen);
    MPCInstance->SetVectorParameterValue(ParamName_SolarColor, LerpedSolar);
    MPCInstance->SetVectorParameterValue(ParamName_IndigoFieldColor, LerpedField);
}

void UkdWorldColorDriver::WriteEdgePulseAlpha(float Alpha) const
{
    if (!MPCInstance) return;
    MPCInstance->SetScalarParameterValue(ParamName_EdgePulseAlpha, Alpha);
}

// ─────────────────────────────────────────────────────────────────────────────
// WriteChromaticAberration
//
// Lightweight single-field write used between blends while the chromatic burst
// is decaying.  Composes the current baseline (lerped from the two profiles
// using the live BlendAlpha) with the active transient burst.  Cheaper than
// re-running ApplyProfileToPostProcess every frame just for this one field.
// ─────────────────────────────────────────────────────────────────────────────

void UkdWorldColorDriver::WriteChromaticAberration() const
{
    if (!PostProcessComp || !ColorTheme) return;

    const float Baseline = FMath::Lerp(
        ColorTheme->LightWorldProfile.ChromaticAberrationIntensity,
        ColorTheme->CrushWorldProfile.ChromaticAberrationIntensity,
        BlendAlpha);

    PostProcessComp->Settings.bOverride_SceneFringeIntensity = true;
    PostProcessComp->Settings.SceneFringeIntensity = Baseline + ChromaticBurstCurrent;
}

void UkdWorldColorDriver::OnInShadowTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    ShadowTintTarget = (NewCount > 0) ? 1.f : 0.f;
    SetComponentTickEnabled(true);
}

void UkdWorldColorDriver::WriteShadowTintAlpha(float Alpha) const
{
    if (!MPCInstance) return;
    MPCInstance->SetScalarParameterValue(ParamName_ShadowTintAlpha, Alpha);
}

bool UkdWorldColorDriver::NeedsTick() const
{
    return bBlending
        || (EdgePulseCurrent > KINDA_SMALL_NUMBER)
        || (ChromaticBurstCurrent > KINDA_SMALL_NUMBER)
        || !FMath::IsNearlyEqual(ShadowTintAlpha, ShadowTintTarget);
}
