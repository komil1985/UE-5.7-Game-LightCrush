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

	// ── Shadow Dash ───────────────────────────────────────────────────────────

	/**
	 * Called by UkdShadowDash ability to apply a burst of velocity along
	 * the shadow plane in the direction the player last steered.
	 *
	 * Falls back to current velocity direction if the player was idle.
	 * Does nothing if no direction can be determined (neither input nor velocity).
	 *
	 * @param Strength  Speed of the burst in cm/s (set by the ability's DashStrength).
	 */
	void ApplyShadowDashImpulse(float Strength);

	/** Cleans up dash state when leaving Shadow2D mode. */
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

protected:
	void PhysShadow2D(float DeltaTime, int32 Iterations);

private:
	/**
	 * Last non-zero input direction on the shadow plane (X=0 always).
	 * Updated every frame in PhysShadow2D when the player provides input.
	 * Persists when braking so the dash always fires in the last intended direction.
	 */
	FVector LastShadowInputDirection = FVector::ZeroVector;

	bool bIsDashing = false;
};
