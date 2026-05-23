// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "kdWorldColorDriver.generated.h"

class APostProcessVolume;
class UMaterialParameterCollectionInstance;
class UPostProcessComponent;
class UkdColorTheme;

// ─────────────────────────────────────────────────────────────────────────────
// UkdWorldColorDriver
//
// Single owner of the world's steady-state look.  Lerps post-process settings
// between two profiles defined on the ColorTheme asset, writes a contract of
// material-parameter-collection scalars/vectors that any material in the
// project can read, and exposes a TriggerEdgePulse() event-driven scalar for
// transient highlight bursts (used by GameFeedbackComponent on shadow entry).
//
// Responsibilities deliberately scoped:
//   ✔ Post-process colour grading, vignette, bloom, DOF (all from profile lerp)
//   ✔ MPC params for materials to react to world state
//   ✔ Public TriggerEdgePulse for transient material highlight events
//   ✘ Camera shakes, time-dilation, smear, niagara  (those live in GameFeedback)
//   ✘ Camera FOV / arm length / mesh scale          (those live in Transition)
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdWorldColorDriver : public UActorComponent
{
    GENERATED_BODY()

public:
    UkdWorldColorDriver();

    /** Heliograph palette + PP profiles.  Required for any colour work. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Driver")
    TObjectPtr<UkdColorTheme> ColorTheme;

    /**
     * Trigger a one-shot edge-trace highlight pulse on all materials reading
     * the EdgePulseAlpha MPC scalar.  Strength is the peak value (typically
     * 1.0); the pulse decays linearly back to zero over Duration seconds.
     *
     * Used by GameFeedbackComponent on shadow-entry / crush-toggle moments.
     */
    UFUNCTION(BlueprintCallable, Category = "Color Driver")
    void TriggerEdgePulse(float Strength = 1.f, float Duration = 0.4f);

    /**
     * Trigger a transient chromatic aberration burst.  Rides additively on top
     * of the steady-state baseline defined per-profile on the ColorTheme, so a
     * crush-mode baseline of 0.4 plus a burst of 3.0 produces a peak of 3.4
     * that decays linearly back to 0.4 over Duration seconds.
     *
     * This is the SOLE entry point for chromatic aberration changes — feedback
     * components must route through here rather than writing SceneFringeIntensity
     * directly, otherwise the steady-state baseline lerp will fight the burst.
     */
    UFUNCTION(BlueprintCallable, Category = "Color Driver")
    void TriggerChromaticBurst(float Intensity = 3.f, float Duration = 0.35f);

    UFUNCTION(BlueprintPure, Category = "Color Driver")
    bool IsBlending() const { return bBlending; }

    /**
     * Current blend alpha (0 = light world, 1 = crush world).  Wire to the
     * HUD's transition-flash colour helper for screen overlay timing.
     */
    UFUNCTION(BlueprintPure, Category = "Color Driver")
    float GetBlendAlpha() const { return BlendAlpha; }

    // ── BP-overridable hooks (optional polish) ───────────────────────────────

    UFUNCTION(BlueprintImplementableEvent, Category = "Color Driver")
    void BP_OnBlendStarted(bool bEnteringCrushMode);

    UFUNCTION(BlueprintImplementableEvent, Category = "Color Driver")
    void BP_OnBlendFinished(bool bInCrushMode);

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    // ── Tag callback ─────────────────────────────────────────────────────────

    UFUNCTION()
    void OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

    // ── Blend internals ──────────────────────────────────────────────────────

    void StartBlend(bool bEnteringCrushMode);
    void ApplyProfileToPostProcess(float Alpha) const;
    void UpdateMPC(float Alpha) const;
    void WriteEdgePulseAlpha(float Alpha) const;
    void WriteChromaticAberration() const;
    bool NeedsTick() const;

    // ── Steady-state blend ───────────────────────────────────────────────────

    float BlendAlpha = 0.f;     // 0 = light, 1 = crush
    float BlendDirection = 0.f;     // +1 toward crush, -1 toward light
    bool  bBlending = false;

    // ── Transient edge pulse ─────────────────────────────────────────────────

    float EdgePulseCurrent = 0.f;
    float EdgePulseDecayRate = 0.f;  // units per second; 0 when no pulse active

    // ── Transient chromatic burst (additive on top of profile baseline) ──────

    float ChromaticBurstCurrent = 0.f;
    float ChromaticBurstDecayRate = 0.f;

    // ── Cached references ────────────────────────────────────────────────────

    UPROPERTY()
    TObjectPtr<UPostProcessComponent> PostProcessComp;

    UPROPERTY()
    TObjectPtr<UMaterialParameterCollectionInstance> MPCInstance;

    UPROPERTY()
    TObjectPtr<APostProcessVolume> CachedShadowVolume;

    UPROPERTY()
    TObjectPtr<APostProcessVolume> CachedLightVolume;

    // ── MPC parameter names (must match the asset) ───────────────────────────

    static const FName ParamName_WorldBlendAlpha;
    static const FName ParamName_EdgePulseAlpha;
    static const FName ParamName_LumenColor;
    static const FName ParamName_SolarColor;
    static const FName ParamName_IndigoFieldColor;
};
