// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "kdCrushStateComponent.generated.h"

class ADirectionalLight;
class UCharacterMovementComponent;
class AkdMyPlayer;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdCrushStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdCrushStateComponent();

	UFUNCTION(BlueprintCallable, Category = "Crush | Physics")
	void ToggleShadowTracking(bool bEnable);

	void HandleVerticalInput(float Value);

	UFUNCTION(BlueprintPure, Category = "Crush | Physics")
	bool IsStandingInShadow() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowCheckFrequency = 0.05f;				// default 20 Hz

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowCheckFrequencyMoving = 0.016f;		// higher frequency when moving

	float LastShadowCheckTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowMoveSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowBrakingDeceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	bool bShowDebugLines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowTraceDistance = 5000.0f;

protected:
	virtual void BeginPlay() override;
	void UpdateShadowPhysics();
	void FindDirectionalLight();
	void ResetPhysicsTo3D();

private:
	FTimerHandle ShadowTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Crush | Lights")
	TObjectPtr<ADirectionalLight> DirectionalLightActor;

	FVector CachedLightDirection;

	TObjectPtr<AkdMyPlayer> CachedOwner = nullptr;

	bool bCanMoveInShadow = false;
	bool bIsInShadow = false;
};
