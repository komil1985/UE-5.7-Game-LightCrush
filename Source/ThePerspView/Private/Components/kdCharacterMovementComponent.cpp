// Copyright ASKD Games


#include "Components/kdCharacterMovementComponent.h"
#include "Player/kdMyPlayer.h"


UkdCharacterMovementComponent::UkdCharacterMovementComponent()
{
    // Required so UE's flying physics sub-system has a non-zero decel to work with.
    // Our custom shadow mode uses ShadowBrakingDeceleration instead, but this
    // prevents accidentally inheriting a zero value if PhysFlying is ever called.
    BrakingDecelerationFlying = 1200.f;
}

void UkdCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
    switch (CustomMovementMode)
    {
    case (uint8)ECustomMovementMode::CMOVE_Shadow2D:
        PhysShadow2D(DeltaTime, Iterations);
        break;

    default:
        Super::PhysCustom(DeltaTime, Iterations);
        break;
    }
}

void UkdCharacterMovementComponent::PhysShadow2D(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME) return;

	// ----------------------------------------------------------------
	// WHY Acceleration AND NOT ConsumeInputVector():
	//
	// UCharacterMovementComponent::PerformMovement() calls ConsumeInputVector()
	// *before* dispatching to any Phys* function. By the time we are here,
	// the input vector has already been consumed and zeroed out. Calling
	// ConsumeInputVector() a second time returns FVector::ZeroVector — which
	// is why Z movement silently did nothing.
	//
	// PerformMovement stores the result as:
	//   Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector))
	//
	// In MOVE_Custom, ConstrainInputAcceleration does NOT strip Z (it only strips
	// Z for walking/falling modes), so Acceleration contains the full 3D input
	// already scaled by MaxAcceleration. This is the correct value to use.
	// ----------------------------------------------------------------

	RestorePreAdditiveRootMotionVelocity(); // Required boilerplate for all custom Phys* functions

	// Extract the shadow-relevant axes from the pre-computed acceleration.
	// X is locked to zero — the character must stay on the shadow plane.
	FVector ShadowAccel = FVector(0.f, Acceleration.Y, Acceleration.Z);

	// Normalize direction so diagonal movement isn't faster than cardinal.
	// Then re-scale by our own ShadowAcceleration value for independent tuning.
	const float AccelSize = ShadowAccel.Size();
	if (AccelSize > KINDA_SMALL_NUMBER)
	{
		ShadowAccel = (ShadowAccel / AccelSize) * ShadowAcceleration;
	}

	const bool bHasInput = AccelSize > KINDA_SMALL_NUMBER;

	if (bHasInput)
	{
		// Accelerate toward ShadowMaxSpeed in the input direction
		Velocity += ShadowAccel * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
	}
	else
	{
		// ----------------------------------------------------------------
		// WHY MANUAL BRAKING INSTEAD OF ApplyVelocityBraking():
		//
		// BrakingDecelerationFlying defaults to 0 in UE, so
		// ApplyVelocityBraking() does nothing unless you set it.
		// Even with BrakingDecelerationFlying set, the function uses
		// FrictionFactor which can misbehave at low speeds and short DeltaTimes.
		//
		// Linear deceleration: subtract a fixed cm/s per second from speed.
		// This is predictable, tunable, and frame-rate stable.
		// ----------------------------------------------------------------
		const float CurrentSpeed = Velocity.Size();
		if (CurrentSpeed > KINDA_SMALL_NUMBER)
		{
			const float SpeedAfterBraking = FMath::Max(0.f, CurrentSpeed - ShadowBrakingDeceleration * DeltaTime);
			Velocity = Velocity.GetSafeNormal() * SpeedAfterBraking;
		}
		else
		{
			Velocity = FVector::ZeroVector; // Hard stop at near-zero to prevent drift
		}
	}

	// Hard-zero X every frame — prevents any drift off the shadow plane from
	// collision responses, root motion residue, or floating point accumulation.
	Velocity.X = 0.f;

	// Standard UE custom physics step: increment iterations, move, handle collision
	Iterations++;
	FVector Delta = Velocity * DeltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);

		// Strip the velocity component pushing into the surface to prevent sticking/jitter
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
		Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
		Velocity.X = 0.f; // Re-zero X after collision response
	}
}
