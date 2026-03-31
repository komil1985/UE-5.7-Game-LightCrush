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
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

protected:
	void PhysShadow2D(float DeltaTime, int32 Iterations);
};
