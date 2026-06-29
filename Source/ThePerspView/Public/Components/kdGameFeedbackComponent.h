// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "kdGameFeedbackComponent.generated.h"

class AkdMyPlayer;
class UCameraComponent;
class UCameraShakeBase;
class UkdAbilitySystemComponent;
class UkdWorldColorDriver;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraSystem;

// ─────────────────────────────────────────────────────────────────────────────
// UkdGameFeedbackComponent
//
// Owns *transient* game-feel only.  After the Heliograph refactor this means:
//
//   ✔ Camera shakes (dash, shadow enter/exit, crush enter/exit/land)
//   ✔ Time-dilation freezes
//   ✔ Edge-pulse triggers       (routed to UkdWorldColorDriver — drives MPC)
//   ✔ Chromatic-burst triggers  (routed to UkdWorldColorDriver — drives PP)
//   ✔ Low-stamina danger vignette pulse (custom PP material)
//   ✔ Shadow-entry vignette burst       (custom PP material)
//   ✔ Character rim-glow lerp           (character DMI)
//   ✔ Dash smear                        (character DMI)
//   ✔ Shadow-entry niagara              (paper-ember burst)
//
// Removed (now owned elsewhere):
//   ✘ Depth-of-Field management         — now owned by WorldColorDriver
//   ✘ Saturation / Contrast writes      — now owned by WorldColorDriver
//
// IMPORTANT: chromatic aberration is back, but this component does NOT write
// SceneFringeIntensity directly.  All chromatic changes route through
// UkdWorldColorDriver::TriggerChromaticBurst() so the steady-state baseline
// (from the ColorTheme profiles) and the transient bursts share a single
// writer — same architectural rule as EdgePulse.
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdGameFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UkdGameFeedbackComponent();

    // ── Public events fired by gameplay code ─────────────────────────────────

    /** Call from the dash ability when the impulse fires.  DashDirection is
     *  the planar shadow-space direction (X must be 0). */
    UFUNCTION(BlueprintCallable, Category = "GameFeel")
    void OnDashPerformed(FVector DashDirection);

    /**
     * Called by Ukd_CrushToggle at the instant the player presses the toggle —
     * BEFORE the anticipation delay.  Drives the immediate "input received"
     * feel: shake, time-freeze, edge pulse.
     */
    UFUNCTION(BlueprintCallable, Category = "GameFeel")
    void OnCrushTransitionStarted(bool bToCrushMode);

    /**
     * Called by Ukd_CrushToggle on the frame the timeline reaches 1.0 —
     * the "landing" thud after the morph completes.  Drives the residual
     * shake and a smaller landing edge-pulse echo.
     */
    UFUNCTION(BlueprintCallable, Category = "GameFeel")
    void OnCrushTransitionFinished(bool bNewCrushMode);

    // =========================================================================
    // Camera shakes
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> DashShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> ShadowEntryShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> ShadowExitShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> CrushEnterShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> CrushExitShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> CrushLandShakeClass;

    // =========================================================================
    // Time-dilation freeze
    // =========================================================================

    /** Time dilation during the freeze (0.05 = world at 5 % speed). */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Freeze",
        meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float FreezeDilation = 0.05f;

    /** Real-world seconds the dilation holds. */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Freeze",
        meta = (ClampMin = "0.0", ClampMax = "0.3"))
    float FreezeDuration = 0.07f;

    // =========================================================================
    // Edge pulse — routed through WorldColorDriver (MPC scalar EdgePulseAlpha)
    //
    // Replaces the old chromatic-peak system.  Materials that read the
    // EdgePulseAlpha MPC scalar can flash their edge glow / exposure / outline
    // intensity on each event.  Strength is the peak; duration is the linear
    // decay window back to zero.
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | EdgePulse",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DashEdgePulseStrength = 0.55f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | EdgePulse",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShadowEntryEdgePulseStrength = 0.85f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | EdgePulse",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrushEnterEdgePulseStrength = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | EdgePulse",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrushExitEdgePulseStrength = 0.75f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | EdgePulse",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrushLandEdgePulseStrength = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | EdgePulse",
        meta = (ClampMin = "0.05", ClampMax = "2.0"))
    float EdgePulseDuration = 0.4f;

    // =========================================================================
    // Chromatic burst — routed through WorldColorDriver (PP SceneFringeIntensity)
    //
    // Additive on top of the steady-state baseline defined per-profile on the
    // ColorTheme (light = 0.0, crush ≈ 0.4 by default).  Strengths below are
    // raw PP intensity values, NOT 0..1 ratios.
    //
    // Range guidance:
    //   0.5 = subtle, 1.5 = noticeable, 3.0 = strong, 5.0 = dizzying.
    //
    // Heliograph defaults lean toward "noticeable on transitions, mild on
    // dash" — the channel split sells the sun-print exposure moment.
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Chromatic",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float DashChromaticBurst = 1.2f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Chromatic",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float ShadowEntryChromaticBurst = 2.5f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Chromatic",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float CrushEnterChromaticBurst = 3.5f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Chromatic",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float CrushExitChromaticBurst = 2.5f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Chromatic",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float CrushLandChromaticBurst = 1.5f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Chromatic",
        meta = (ClampMin = "0.05", ClampMax = "2.0"))
    float ChromaticBurstDuration = 0.35f;

    // =========================================================================
    // Custom post-process material (vignette pulses only — chromatic removed)
    //
    // Assign a PP material with these scalar params:
    //   VignetteIntensity   — drives the low-stamina danger pulse
    //   RimIntensity        — drives optional screen-edge rim flicker
    //
    // (The character mesh's own DMI handles its character-local rim and smear.)
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess")
    TObjectPtr<UMaterialInterface> CrushPostProcessMaterial;

    // ── Vignette pulse (low-stamina danger) ──────────────────────────────────

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LowStaminaThreshold = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaxVignetteIntensity = 0.7f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.5", ClampMax = "10.0"))
    float VignettePulseFrequency = 2.5f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "1.0"))
    float VignetteLerpSpeed = 5.0f;

    // ── Shadow-entry vignette burst ──────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InShadowVignetteStrength = 0.45f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "1.0"))
    float ShadowVignetteLerpSpeed = 4.0f;

    // =========================================================================
    // Character rim glow (writes to character mesh DMI)
    //
    // In Heliograph the rim represents the lumen "exposure" line on the
    // character silhouette.  The character material should multiply its rim
    // emissive by the RimColor vector param so the colour comes from the
    // theme, not hard-coded in the material.
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Rim",
        meta = (ClampMin = "0.0", ClampMax = "3.0"))
    float RimPeakIntensity = 1.2f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Rim",
        meta = (ClampMin = "1.0"))
    float RimLerpSpeed = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Rim")
    FName RimIntensityParamName = FName("RimIntensity");

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Rim")
    FName RimColorParamName = FName("RimColor");

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Rim",
        meta = (ClampMin = "0"))
    int32 RimMaterialSlotIndex = 0;

    // =========================================================================
    // Dash smear (vertex stretch on character mesh DMI)
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Smear")
    FName SmearStrengthParamName = FName("SmearStrength");

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Smear")
    FName SmearDirectionParamName = FName("SmearDirection");

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Smear",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DashSmearPeak = 0.85f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Smear",
        meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float SmearDecaySpeed = 8.0f;

    // =========================================================================
    // Shadow-entry niagara (paper-ember burst — retuned for Heliograph)
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Particles")
    TObjectPtr<UNiagaraSystem> ShadowEntryNiagara;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Particles",
        meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float ShadowEntryNiagaraScale = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Particles",
        meta = (ClampMin = "0.0"))
    float ShadowEntryNiagaraZOffset = 60.0f;

    /** Called by CrushToggle ability / CrushStateComponent at transition start */
    void NotifyCrushTransitionStarted(bool bEntering);

    /** Called by CrushToggle ability / CrushStateComponent at transition finish */
    void NotifyCrushTransitionFinished(bool bNowInCrush);

    /** Called when crush is denied (stamina empty, blocked, etc.) */
    void NotifyCrushDenied();

    /** Called when player lands after exiting crush (CrushLand SFX) */
    void NotifyCrushLand();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

private:
    /** Resolves AudioSubsystem once and caches it — safe to call any frame after BeginPlay */
    class UkdAudioSubsystem* GetAudioSubsystem() const;

    // ── Cached refs ──────────────────────────────────────────────────────────

    UPROPERTY() TObjectPtr<UkdAbilitySystemComponent>  CachedASC;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic>   PPInstance;     // vignette pulses
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic>   CharMeshDMI;    // rim + smear
    UPROPERTY() TObjectPtr<UCameraComponent>           CachedCamera;
    UPROPERTY() TObjectPtr<UkdWorldColorDriver>        CachedWorldColorDriver;

    // ── State (vignette / rim) ───────────────────────────────────────────────

    float CurrentVignette = 0.f;
    float TargetVignette = 0.f;
    float VignettePhase = 0.f;
    float CurrentStaminaFrac = 1.f;

    float CurrentShadowVignette = 0.f;
    float TargetShadowVignette = 0.f;

    float CurrentRimIntensity = 0.f;
    float TargetRimIntensity = 0.f;

    // ── State (smear) ────────────────────────────────────────────────────────

    float   CurrentSmear = 0.f;
    FVector LastDashDirection = FVector::ZeroVector;

    // ── State (time freeze) ──────────────────────────────────────────────────

    float FreezeStartRealTime = -1.f;

    // ── Mode ─────────────────────────────────────────────────────────────────

    bool bInCrushMode = false;

    // ── Tag callbacks ────────────────────────────────────────────────────────

    UFUNCTION() void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);
    UFUNCTION() void OnInShadowTagChanged(const FGameplayTag Tag, int32 NewCount);
    UFUNCTION() void OnExhaustedTagChanged(const FGameplayTag Tag, int32 NewCount);

    /** Stamina attribute change — feeds the low-stamina vignette pulse threshold. */
    void OnStaminaChanged(const struct FOnAttributeChangeData& Data);

    // ── Internal helpers ─────────────────────────────────────────────────────

    void PlayShake(TSubclassOf<UCameraShakeBase> ShakeClass) const;
    void StartTimeFreeze();
    void PulseEdge(float Strength) const;
    void PulseChromatic(float Intensity) const;

    void WritePP_Vignette(float Value) const;
    void WritePP_BlendWeight(float Weight) const;

    void WriteMesh_Rim(float Intensity) const;       // character DMI rim
    void WriteMesh_RimColor(const FLinearColor& Color) const;
    void WriteMesh_Smear(float Strength, FVector PlanarDirection) const;

    void SpawnShadowEntryNiagara();

    float ReadStaminaFraction() const;
    void  UpdateVignettePulse(float DeltaTime);
    void  UpdateRim(float DeltaTime);
    void  UpdateSmear(float DeltaTime);

    bool  NeedsTick() const;
    void  SetTickActive(bool bActive);
};
