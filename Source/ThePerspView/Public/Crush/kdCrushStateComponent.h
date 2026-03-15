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
	float ShadowMoveSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowBrakingDeceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	bool bShowDebugLines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowTraceDistance = 5000.0f;
	//////////////////////////////////////////////////////////////////////////

	// Stamina handling drain / regen settings ///////////////////////////////
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float StaminaDrainRate = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float StaminaRegenRate = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float RegenDelay = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	TSubclassOf<UGameplayEffect> StaminaModEffectClass;

	UPROPERTY(BlueprintAssignable, Category = "Stamina")
	FOnDrainStateChanged OnDrainStateChanged;
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

	UFUNCTION()
	void ApplyStaminaDelta(float Delta);	// Positive for regen, negative for drain

	TObjectPtr<AkdMyPlayer> CachedOwner = nullptr;
	float TimeSinceLastShadowCheck = 0.0f;
	float TimeSinceLastMove = 0.0f;
	FVector CachedLightDirection;
	bool bIsInShadow = false;
	bool bWasDraining = false;
	
};
