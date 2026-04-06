// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "kdCharacterMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API UkdCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:
	UkdCharacterMovementComponent();

	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

	UPROPERTY(EditDefaultsOnly, Category = "Shadow Movement", meta = (ClampMin = "0.0"))
	float ShadowMaxSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shadow Movement", meta = (ClampMin = "0.0"))
	float ShadowBrakingDeceleration = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shadow Movement", meta = (ClampMin = "0.0"))
	float ShadowAcceleration = 1200.f;

protected:
	void PhysShadow2D(float DeltaTime, int32 Iterations);
};
