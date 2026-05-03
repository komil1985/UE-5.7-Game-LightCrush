// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "kdGameFeedbackComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// UkdGameFeedbackComponent
//
// Manages all screen-space feedback for player state changes:
//   • Chromatic aberration spikes (dash, crush, shadow entry)
//   • Vignette pulse (low stamina, shadow entry)
//   • Rim glow (mesh material, shadow entry)
//   • Time-dilation freeze (crush toggle)
//   • Depth of field blend (3D → 2D crush diorama look)
//   • Color saturation + contrast grade (2D mode graphic look)
//
// DOF and color grading write directly to the camera's PostProcessSettings —
// no extra material assets required.  The PP material (CrushPostProcessMaterial)
// handles only chromatic aberration and vignette.
// ─────────────────────────────────────────────────────────────────────────────

class UkdAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UCameraShakeBase;
class UCameraComponent;
struct FOnAttributeChangeData;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdGameFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
    UkdGameFeedbackComponent();

    // =========================================================================
    // Public API
    // =========================================================================

    UFUNCTION(BlueprintCallable, Category = "GameFeel")
    void OnDashPerformed();

    /** Called the instant the crush button registers (before anticipation delay). */
    void OnCrushTransitionStarted(bool bToCrushMode);

    /** Called when the main morph finishes (before EndAbility). */
    void OnCrushTransitionFinished(bool bNewCrushMode);

    // =========================================================================
    // Camera Shake Classes
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> DashShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> ShadowEntryShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> ShadowExitShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> StaminaEmptyShakeClass;

    /** Heavy shake on crush-enter button press. */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> CrushEnterShakeClass;

    /** Medium shake on crush-exit button press. */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> CrushExitShakeClass;

    /** Light landing shake when morph completes. */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
    TSubclassOf<UCameraShakeBase> CrushLandShakeClass;

    // =========================================================================
    // Time-dilation freeze
    // =========================================================================

    /** Global time dilation at freeze moment (0.05 = world at 5 % speed). */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Freeze",
        meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float FreezeDilation = 0.05f;

    /** Real-world seconds the dilation lasts. */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Freeze",
        meta = (ClampMin = "0.0", ClampMax = "0.3"))
    float FreezeDuration = 0.07f;

    // =========================================================================
    // Post-process material (chromatic + vignette)
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess")
    TObjectPtr<UMaterialInterface> CrushPostProcessMaterial;

    // ── Chromatic ─────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float DashChromaticPeak = 1.5f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float ShadowEntryChromaticPeak = 2.5f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float CrushEnterChromaticPeak = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float CrushExitChromaticPeak = 2.2f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float CrushLandChromaticPeak = 1.4f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "1.0"))
    float ChromaticDecaySpeed = 6.0f;

    // ── Vignette (low-stamina pulse) ──────────────────────────────────────────

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

    // ── Shadow vignette ───────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InShadowVignetteStrength = 0.45f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
        meta = (ClampMin = "1.0"))
    float ShadowVignetteLerpSpeed = 4.0f;

    // ── Rim glow ──────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shadow | Rim",
        meta = (ClampMin = "0.0", ClampMax = "3.0"))
    float RimPeakIntensity = 1.2f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shadow | Rim",
        meta = (ClampMin = "1.0"))
    float RimLerpSpeed = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shadow | Rim")
    FName RimIntensityParamName = FName("RimIntensity");

    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shadow | Rim",
        meta = (ClampMin = "0"))
    int32 RimMaterialSlotIndex = 0;

    // =========================================================================
    // Depth of field (2D diorama look) — written to camera PP settings
    // =========================================================================

    /** Enable DOF lerp when entering / exiting Crush mode. */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Camera | DOF")
    bool bManageCrushDOF = true;

    /** Cinematic f-stop in 2D mode (lower = more blur, 1.4–2.8 for diorama). */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Camera | DOF",
        meta = (ClampMin = "0.5", ClampMax = "22.0"))
    float Crush2DFStop = 2.0f;

    /** Sensor width (mm) used with f-stop for blur calculation. */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Camera | DOF",
        meta = (ClampMin = "1.0"))
    float Crush2DSensorWidth = 150.f;

    // =========================================================================
    // Color grading (2D mode graphic look) — written to camera PP settings
    // =========================================================================

    /** Saturation multiplier in 2D mode (0.80–0.88 for a flat, graphic feel). */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Camera | Color",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Crush2DSaturation = 0.82f;

    /** Contrast multiplier in 2D mode (1.05–1.12 crispens the shadow silhouettes). */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Camera | Color",
        meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float Crush2DContrast = 1.08f;

    /** Lerp speed for DOF and color grading transitions. */
    UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Camera | Color",
        meta = (ClampMin = "0.5"))
    float CameraGradeLerpSpeed = 5.f;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    // ── Cached refs ───────────────────────────────────────────────────────────
    UPROPERTY() TObjectPtr<UkdAbilitySystemComponent> CachedASC;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic>  PPInstance;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic>  CharMeshDMI;
    UPROPERTY() TObjectPtr<UCameraComponent>          CachedCamera;

    // ── Chromatic / vignette state ────────────────────────────────────────────
    float CurrentChromatic = 0.f;
    float CurrentVignette = 0.f;
    float TargetVignette = 0.f;
    float VignettePhase = 0.f;
    float CurrentStaminaFrac = 1.f;
    bool  bInCrushMode = false;
    float CurrentRimIntensity = 0.f;
    float TargetRimIntensity = 0.f;
    float CurrentShadowVignette = 0.f;
    float TargetShadowVignette = 0.f;

    // ── Time-dilation ─────────────────────────────────────────────────────────
    float FreezeStartRealTime = -1.f;

    // ── Camera PP state (DOF + color grade) ───────────────────────────────────
    // We store current and target for each grade param and lerp in Tick.
    // 3D baselines are restored when crush mode exits.

    float CurrentFStop = 0.f;    // 0 = DOF off (UE default)
    float TargetFStop = 0.f;

    float CurrentSensorWidth = 0.f;
    float TargetSensorWidth = 0.f;

    float CurrentSaturation = 1.f;
    float TargetSaturation = 1.f;

    float CurrentContrast = 1.f;
    float TargetContrast = 1.f;

    void WriteCameraGrade();   // writes all four values to camera PP settings

    // ── Deferred PP hide (crush exit echo) ───────────────────────────────────
    FTimerHandle PPHideTimerHandle;
    void SchedulePPHide();
    void HidePP();

    // ── Tag / attribute callbacks ─────────────────────────────────────────────
    void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);
    void OnInShadowTagChanged(const FGameplayTag Tag, int32 NewCount);
    void OnExhaustedTagChanged(const FGameplayTag Tag, int32 NewCount);
    void OnStaminaChanged(const FOnAttributeChangeData& Data);

    // ── Helpers ───────────────────────────────────────────────────────────────
    void PlayShake(TSubclassOf<UCameraShakeBase> ShakeClass);
    void WritePP_Chromatic(float Value);
    void WritePP_Vignette(float Value);
    void WritePP_BlendWeight(float Weight);
    void WritePP_Rim(float Value);
    bool TryGetPPInstance();
    void SetTickActive(bool bActive);
    bool NeedsTick() const;
		
};
