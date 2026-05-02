// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "kdGameFeedbackComponent.generated.h"

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

	// ── Public API (called by abilities / movement code) ─────────────────────

	// ── Called externally by ShadowDash ability ───────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "GameFeel")
	void OnDashPerformed();

	// ── Post-process material to instantiate ─────────────────────────────────
	// Assign M_CrushPostProcess in BP_Player Details panel.
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess")
	TObjectPtr<UMaterialInterface> CrushPostProcessMaterial;

	// ── Camera Shake Classes — assign in BP_Player Details ───────────────────
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> DashShakeClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> ShadowEntryShakeClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> ShadowExitShakeClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> StaminaEmptyShakeClass;

	/** Heavy shake at the moment the player initiates entering crush mode. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> CrushEnterShakeClass;

	/** Shake at the moment the player initiates returning to 3D. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> CrushExitShakeClass;

	/** Lighter "landing" shake that fires when the morph completes. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> CrushLandShakeClass;

	// ── Chromatic Aberration ─────────────────────────────────────────────────
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float DashChromaticPeak = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ShadowEntryChromaticPeak = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "1.0"))
	float ChromaticDecaySpeed = 6.0f;

	/** Chromatic spike when the player first presses the crush-enter button. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float CrushEnterChromaticPeak = 3.0f;

	/** Chromatic spike when the player first presses the crush-exit button. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float CrushExitChromaticPeak = 2.2f;

	/** Smaller echo spike when the main morph finishes (landing pop). */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float CrushLandChromaticPeak = 1.4f;

	// ── Vignette ─────────────────────────────────────────────────────────────
	// Pulses when stamina drops below LowStaminaThreshold in 2D mode.
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

	// ── Shadow Vignette (screen darkens when player enters shadow) ────────────

	/** Vignette intensity applied when player IS standing in shadow.
	*  Stacks additively on top of the low-stamina vignette. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InShadowVignetteStrength = 0.45f;

	/** How fast the shadow vignette fades in/out (higher = snappier). */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess", meta = (ClampMin = "1.0"))
	float ShadowVignetteLerpSpeed = 4.0f;

	// ── 2. Rim Glow (character mesh material) ────────────────────────────────

	/** Peak rim glow intensity when fully in shadow. 0 = disabled. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shadow | Rim", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float RimPeakIntensity = 1.2f;

	/** Fade speed for the rim glow. Higher = snappier. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shadow | Rim", meta = (ClampMin = "1.0"))
	float RimLerpSpeed = 8.0f;

	/** Scalar parameter name in your character mesh material for rim intensity. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shadow | Rim")
	FName RimIntensityParamName = FName("RimIntensity");

	/** Which material slot index on the skeletal mesh carries the rim param. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shadow | Rim", meta = (ClampMin = "0"))
	int32 RimMaterialSlotIndex = 0;

	// ── Crush-mode transition hooks (called by kd_CrushToggle) ───────────────

	/**
	* Call at the very START of a crush toggle — before the anticipation delay.
	*
	* Entering crush  (bToCrushMode = true):
	*   • Makes the post-process layer visible early so the chromatic spike
	*     renders even before State.CrushMode is officially added.
	*   • Spikes chromatic to CrushEnterChromaticPeak.
	*   • Plays CrushEnterShakeClass.
	*
	* Exiting crush (bToCrushMode = false):
	*   • PP is already visible; just spikes chromatic + plays exit shake.
	*/
	void OnCrushTransitionStarted(bool bToCrushMode);

	/**
	 * Call when the MAIN MORPH finishes (OnTransitionFinished in the ability,
	 * before EndAbility).
	 *
	 * Plays a smaller chromatic echo ("landing pop") + CrushLandShakeClass.
	 * The echo decays naturally via tick; the PP blend weight is hidden later
	 * via a timer (on crush exit) so the echo remains visible long enough
	 * to read.
	 */
	void OnCrushTransitionFinished(bool bNewCrushMode);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ── Cached references ────────────────────────────────────────────────────
	UPROPERTY()
	TObjectPtr<UkdAbilitySystemComponent> CachedASC;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PPInstance;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CharMeshDMI;

	UPROPERTY()
	TObjectPtr<UCameraComponent> CachedCamera;

	// ── Runtime state ────────────────────────────────────────────────────────
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

	// ── Tag callbacks ────────────────────────────────────────────────────────
	void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnInShadowTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnExhaustedTagChanged(const FGameplayTag Tag, int32 NewCount);

	// ── Attribute callback ───────────────────────────────────────────────────
	void OnStaminaChanged(const FOnAttributeChangeData& Data);

	// ── Helpers ──────────────────────────────────────────────────────────────
	void PlayShake(TSubclassOf<UCameraShakeBase> ShakeClass);
	void WritePP_Chromatic(float Value);
	void WritePP_Vignette(float Value);
	void WritePP_BlendWeight(float Weight);
	bool TryGetPPInstance();
	void SetTickActive(bool bActive);
	bool NeedsTick() const;
	void WritePP_Rim(float Value);


	// ── PP blend-weight deferred hide (used when exiting crush) ───────────────
	//
	// On crush exit, the chromatic echo needs to remain visible for a moment
	// after State.CrushMode is removed.  Rather than hiding PP immediately in
	// OnCrushModeTagChanged, we schedule a timer for when the echo has decayed.
	FTimerHandle PPHideTimerHandle;

	void SchedulePPHide();   // starts PPHideTimerHandle
	void HidePP();           // fires when timer expires — zeros and hides the layer
		
};
