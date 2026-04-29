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

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

	// ── In-Shadow Rim Glow ────────────────────────────────────────────────────

/** Peak rim glow intensity when fully in shadow. Set to 0 to disable. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float InShadowRimPeak = 1.2f;

	/** Speed at which rim intensity lerps in/out. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess", meta = (ClampMin = "1.0"))
	float RimLerpSpeed = 10.0f;

	/** Name of the scalar parameter in your post-process material that controls rim intensity. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess")
	FName RimIntensityParamName = FName("RimIntensity");

private:
	// ── Cached references ────────────────────────────────────────────────────
	UPROPERTY()
	TObjectPtr<UkdAbilitySystemComponent> CachedASC;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PPInstance;

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

		
};
