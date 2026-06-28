// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "kdWorldColorDriver.generated.h"


USTRUCT(BlueprintType)
struct FkdPostProcessModePreset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "kd|PostProcess")
    float BloomIntensity = 1.0f;

    UPROPERTY(EditAnywhere, Category = "kd|PostProcess")
    float VignetteIntensity = 0.4f;

    // Baseline chromatic aberration; GameFeedbackComponent adds transient
    // bursts on top of this, both ultimately written by the driver.
    UPROPERTY(EditAnywhere, Category = "kd|PostProcess")
    float SceneFringeIntensity = 0.0f;

    UPROPERTY(EditAnywhere, Category = "kd|PostProcess")
    float WhiteTemp = 6500.0f;

    UPROPERTY(EditAnywhere, Category = "kd|PostProcess")
    float FilmSlope = 0.88f;

    UPROPERTY(EditAnywhere, Category = "kd|PostProcess")
    float FilmToe = 0.55f;
};


class APostProcessVolume;
class UMaterialParameterCollectionInstance;
class UPostProcessComponent;
class UkdColorTheme;
class UCameraComponent;

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

    UPROPERTY(EditDefaultsOnly, Category = "kd|PostProcess")
    FkdPostProcessModePreset LightWorldPostProcess;

    UPROPERTY(EditDefaultsOnly, Category = "kd|PostProcess")
    FkdPostProcessModePreset CrushModePostProcess;

    UFUNCTION(BlueprintCallable, Category = "Color Driver")
    void ApplyBlendedPostProcess(float WorldBlendAlpha);

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

    /** Seconds for the player shadow-tint to fade in/out on State.InShadow
    *  changes.  Short — it should feel like ink soaking in, not a scene blend. */
    UPROPERTY(EditDefaultsOnly, Category = "Color Driver|Shadow Tint",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShadowTintFadeDuration = 0.18f;

    /** Current shadow-tint alpha (0 = lit / 3D, 1 = fully in shadow in Crush).
     *  Materials read the matching MPC scalar; this getter is for UI/debug. */
    UFUNCTION(BlueprintPure, Category = "Color Driver")
    float GetShadowTintAlpha() const { return ShadowTintAlpha; }

    /**
    * Survey-alpha hook. The strategic camera broadcasts its 0..1 alpha here; the
    * driver folds it into the tilt-shift band (deeper blur + tighter focus = the
    * "model on a surveyor's plate" look) and writes the MPC. Routing through the
    * driver keeps it the SOLE MPC writer — the strategic camera never touches the
    * collection itself.
    */
    void NotifySurveyAlpha(float Alpha);

    // ── BP-overridable hooks (optional polish) ───────────────────────────────

    UFUNCTION(BlueprintImplementableEvent, Category = "Color Driver")
    void BP_OnBlendStarted(bool bEnteringCrushMode);

    UFUNCTION(BlueprintImplementableEvent, Category = "Color Driver")
    void BP_OnBlendFinished(bool bInCrushMode);

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    /** Added to TiltBlurScale at full survey. Gameplay band stays as-tuned at rest. */
    UPROPERTY(EditDefaultsOnly, Category = "Tilt Shift|Survey",
        meta = (ClampMin = "0.0", ClampMax = "0.8"))
    float SurveyBlurBoost = 0.45f;

    /** TiltFocusWidth multiplier at full survey (<1 narrows the band into a stripe). */
    UPROPERTY(EditDefaultsOnly, Category = "Tilt Shift|Survey",
        meta = (ClampMin = "0.2", ClampMax = "1.0"))
    float SurveyWidthScale = 0.65f;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCameraComponent> CachedCameraComponent = nullptr;

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

    // ── Shadow tint (player-in-shadow, Crush Mode only) ──────────────────────

    UFUNCTION()
    void OnInShadowTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

    void WriteShadowTintAlpha(float Alpha) const;

    float ShadowTintAlpha = 0.f;   // current, written to MPC
    float ShadowTintTarget = 0.f;   // 1 while State.InShadow is present

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
    static const FName ParamName_ShadowTintAlpha;

    static const FName ParamName_TiltFocusCenter;
    static const FName ParamName_TiltFocusWidth;
    static const FName ParamName_TiltBlurScale;
    static const FName ParamName_TiltFocusFalloff;

    /** Live survey alpha pushed by the strategic camera (0 at rest). */
    float TiltSurveyAlpha = 0.f;

    /** Composes profile-lerped band + survey ramp, writes all four MPC scalars. */
    void WriteTiltShift() const;
};
