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

void UkdCharacterMovementComponent::ApplyShadowDashImpulse(float Strength)
{
	// Prefer last steering direction; fall back to current velocity if the
	// player pressed dash while idle
	FVector DashDir = LastShadowInputDirection;

	if (DashDir.IsNearlyZero())
	{
		DashDir = FVector(0.f, Acceleration.Y, Acceleration.Z);
		DashDir.X = 0.f;
		DashDir = DashDir.GetSafeNormal();
	}

	// Still no direction — player is completely stationary with no prior input
	if (DashDir.IsNearlyZero())
	{
		DashDir = FVector(0.f, Velocity.Y, Velocity.Z);
		DashDir.X = 0.f;
		DashDir = DashDir.GetSafeNormal();
	}

	if (DashDir.IsNearlyZero()) return;

	bIsDashing = true;

	// Override (don't add to) velocity so the burst is always a predictable speed
	Velocity = DashDir * Strength;
	Velocity.X = 0.f;
}

void UkdCharacterMovementComponent::PhysShadow2D(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME) return;

	RestorePreAdditiveRootMotionVelocity();

	// Acceleration is populated by PerformMovement() from ConsumeInputVector() before
	// any Phys* function runs — reading it here gives the full 2D shadow input (Y + Z).
	// X is zeroed to keep the character locked to the shadow plane.
	FVector ShadowAccel = FVector(0.f, Acceleration.Y, Acceleration.Z);

	const float AccelSize = ShadowAccel.Size();
	const bool bHasInput = AccelSize > KINDA_SMALL_NUMBER;

	if (bHasInput)
	{
		// Cache direction for dash fallback — persists after input stops
		LastShadowInputDirection = ShadowAccel.GetSafeNormal();
		LastShadowInputDirection.X = 0.0f;

		// Normalise then re-scale by our own acceleration value so diagonal
		// movement never exceeds the same speed as cardinal movement
		ShadowAccel = LastShadowInputDirection * ShadowAcceleration;
		Velocity += ShadowAccel * DeltaTime;
		//Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
		if (!bIsDashing)
		{
			Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
		}
		else if (Velocity.Size() <= ShadowMaxSpeed)
		{
			// Dash has naturally decelerated to normal movement speed
			bIsDashing = false;
		}
	}
	else
	{
		// Linear braking: subtract a fixed cm/s per second from current speed.
		// More predictable than ApplyVelocityBraking() which relies on
		// BrakingDecelerationFlying defaulting to non-zero (it doesn't in UE).
		const float CurrentSpeed = Velocity.Size();
		if (CurrentSpeed > KINDA_SMALL_NUMBER)
		{
			const float BrakedSpeed = FMath::Max(0.f, CurrentSpeed - ShadowBrakingDeceleration * DeltaTime);
			Velocity = Velocity.GetSafeNormal() * BrakedSpeed;

			// Clear dash flag once the burst has decelerated to normal range
			if (BrakedSpeed <= ShadowMaxSpeed)
			{
				bIsDashing = false;
			}
		}
		else
		{
			Velocity = FVector::ZeroVector;
			bIsDashing = false;
		}
	}

	// Hard-zero X every frame — prevents drift from collisions or FP accumulation
	Velocity.X = 0.f;

	Iterations++;
	FVector Delta = Velocity * DeltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
		Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
		Velocity.X = 0.f;

		bIsDashing = false;
	}
}
