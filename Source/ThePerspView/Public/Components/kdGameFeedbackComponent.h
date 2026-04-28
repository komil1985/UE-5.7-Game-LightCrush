// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "kdGameFeedbackComponent.generated.h"

class UkdAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UCameraShakeBase;
struct FOnAttributeChangeData;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdGameFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdGameFeedbackComponent();

	// ── Public API (called by abilities / movement code) ─────────────────────

	/** Call from UkdShadowDash::ActivateAbility after ApplyShadowDashImpulse(). */
	UFUNCTION(BlueprintCallable, Category = "GameFeel")
	void OnDashPerformed();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

	// ── Camera Shake Classes ─────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> DashShakeClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> ShadowEntryShakeClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> ShadowExitShakeClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | Shakes")
	TSubclassOf<UCameraShakeBase> StaminaEmptyShakeClass;

	// ── Chromatic Aberration ─────────────────────────────────────────────────

	/** Peak aberration value injected on dash. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float DashChromaticPeak = 1.5f;

	/** Peak aberration value injected on shadow entry. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ShadowEntryChromaticPeak = 2.5f;

	/** How fast chromatic aberration decays back to zero (units/sec). */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "1.0"))
	float ChromaticDecaySpeed = 6.0f;

	// ── Vignette ─────────────────────────────────────────────────────────────

	/** Stamina fraction below which the vignette pulse starts (0-1). */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowStaminaThreshold = 0.35f;

	/** Maximum vignette intensity at zero stamina. */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxVignetteIntensity = 0.7f;

	/** Pulse frequency in Hz (oscillations per second). */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float VignettePulseFrequency = 2.5f;

	/** Lerp speed for vignette transitions (in/out). */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeel | PostProcess",
		meta = (ClampMin = "1.0"))
	float VignetteLerpSpeed = 5.0f;

private:
	// ── Internal state ───────────────────────────────────────────────────────

	UPROPERTY()
	TObjectPtr<UkdAbilitySystemComponent> CachedASC;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CachedPPMaterial;

	// Post-process runtime values
	float CurrentChromatic = 0.f;   // decays to 0 each tick
	float CurrentVignette = 0.f;   // lerps toward TargetVignette
	float TargetVignette = 0.f;   // set by stamina change callback
	float VignettePhase = 0.f;   // accumulates for sin pulse
	float CurrentStaminaFrac = 1.f;   // 0-1, updated by attribute delegate

	// ── Tag event callbacks ──────────────────────────────────────────────────

	UFUNCTION()
	void OnInShadowTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UFUNCTION()
	void OnExhaustedTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// ── Attribute callback ───────────────────────────────────────────────────

	void OnStaminaChanged(const FOnAttributeChangeData& Data);

	// ── Helpers ──────────────────────────────────────────────────────────────

	void PlayCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass);
	void SetChromaticAberration(float Value);
	void SetVignetteIntensity(float Value);
	void SetTickEnabled(bool bEnabled);
	bool NeedsTickThisFrame() const;

		
};
