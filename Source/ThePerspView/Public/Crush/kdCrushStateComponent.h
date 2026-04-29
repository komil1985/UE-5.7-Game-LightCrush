// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "kdCrushStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDrainStateChanged, bool, bIsDraining);

class ADirectionalLight;
class UCharacterMovementComponent;
class AkdMyPlayer;
class UGameplayEffect;
struct FGameplayTag;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdCrushStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdCrushStateComponent();

	// Shadow tracking and movements settings /////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "Crush | Physics")
	void ToggleShadowTracking(bool bEnable);

	void HandleVerticalInput(float Value);

	UFUNCTION(BlueprintPure, Category = "Crush | Physics")
	bool IsStandingInShadow() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowCheckFrequency = 0.05f;				// default 20 Hz

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowCheckFrequencyMoving = 0.016f;		// higher frequency when moving

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowJumpPower = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowBrakingDeceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	bool bShowDebugLines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowTraceDistance = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowMoveSpeed = 300.0f;
	//////////////////////////////////////////////////////////////////////////

	// Stamina handling drain / regen settings ///////////////////////////////
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float BaseStaminaDrainRate = 5.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float ShadowStaminaDrainRate = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	TSubclassOf<UGameplayEffect> StaminaModEffectClass;

	UPROPERTY(BlueprintAssignable, Category = "Stamina")
	FOnDrainStateChanged OnDrainStateChanged;

	// ── Stamina Regen (3D only, delayed) ────────────────────────────────────────

	/** How long after exiting Crush Mode before regen begins (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float RegenDelay = 2.0f;

	/** Stamina points restored per second during regen. */
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float StaminaRegenRate = 5.0f;

	/** Timer firing frequency for regen ticks. Lower = smoother, more GAS calls. */
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float RegenTickInterval = 0.1f;
	//////////////////////////////////////////////////////////////////////////

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void FindDirectionalLight();
	void ResetPhysicsTo3D();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Crush | Lights")
	TObjectPtr<ADirectionalLight> DirectionalLightActor;

	UPROPERTY(EditDefaultsOnly, Category = "Crush")
	float CrushGravityScale = 0.25f;

	UPROPERTY()
	TObjectPtr<AkdMyPlayer> CachedOwner = nullptr;

	UFUNCTION()
	void ApplyStaminaDelta(float Delta);	// Positive for regen, negative for drain

	void OnCrushModeTagChanged_Regen(const FGameplayTag Tag, int32 NewCount);
	void BeginRegen();
	void RegenTick();
	void StopRegen();

	FTimerHandle RegenDelayHandle;
	FTimerHandle RegenTickHandle;
	bool bIsRegenerating = false;
	float TimeSinceLastShadowCheck = 0.0f;
	float TimeSinceLastMove = 0.0f;
	FVector CachedLightDirection;
	bool bIsInShadow = false;
	bool bWasDraining = false;
	bool bWasMoving = false;
};
