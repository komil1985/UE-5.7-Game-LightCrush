// Copyright ASKD Games


#include "Components/kdCharacterMovementComponent.h"
#include "Player/kdMyPlayer.h"
#include "Crush/kdCrushDirectionLibrary.h"


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
	//// Prefer last steering direction; fall back to current velocity if the
	//// player pressed dash while idle
	//FVector DashDir = LastShadowInputDirection;

	//if (DashDir.IsNearlyZero())
	//{
	//	DashDir = FVector(0.f, Acceleration.Y, Acceleration.Z);
	//	DashDir.X = 0.f;
	//	DashDir = DashDir.GetSafeNormal();
	//}

	//// Still no direction — player is completely stationary with no prior input
	//if (DashDir.IsNearlyZero())
	//{
	//	DashDir = FVector(0.f, Velocity.Y, Velocity.Z);
	//	DashDir.X = 0.f;
	//	DashDir = DashDir.GetSafeNormal();
	//}

	//if (DashDir.IsNearlyZero()) return;

	//bIsDashing = true;

	//// Override (don't add to) velocity so the burst is always a predictable speed
	//Velocity = DashDir * Strength;
	//Velocity.X = 0.f;

	const FVector CollapseN = GetCrushCollapseNormal();

	// Prefer last steering direction; fall back to input, then velocity.
	FVector DashDir = LastShadowInputDirection;

	if (DashDir.IsNearlyZero())
	{
		DashDir = FVector::VectorPlaneProject(Acceleration, CollapseN).GetSafeNormal();
	}
	if (DashDir.IsNearlyZero())
	{
		DashDir = FVector::VectorPlaneProject(Velocity, CollapseN).GetSafeNormal();
	}
	if (DashDir.IsNearlyZero()) return;

	// Enter an EXPLICIT, TIME-BOUNDED dash state. From here until DashTimeRemaining
	// reaches zero, PhysShadow2D owns velocity outright — steering input can re-aim
	// intent for the hand-off but is physically incapable of adding speed. This is
	// the structural fix for the runaway-speed bug, where the old code let
	// `Velocity += ShadowAccel * dt` pump speed every frame the dash flag was set.
	bIsDashing = true;
	DashDirection = DashDir;                                 // locked for the burst
	DashInitialSpeed = FMath::Max(Strength, ShadowMaxSpeed);    // never below walk speed
	DashTimeRemaining = DashDuration;

	// Override velocity for a predictable burst, kept on the play plane.
	Velocity = DashDirection * DashInitialSpeed;
	Velocity = FVector::VectorPlaneProject(Velocity, CollapseN);
}

void UkdCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	const bool bWasShadow =
		(PreviousMovementMode == MOVE_Custom) &&
		(PreviousCustomMode == (uint8)ECustomMovementMode::CMOVE_Shadow2D);

	const bool bIsShadowNow =
		(MovementMode == MOVE_Custom) &&
		(CustomMovementMode == (uint8)ECustomMovementMode::CMOVE_Shadow2D);

	if (bWasShadow && !bIsShadowNow)
	{
		// Exited the shadow plane — clear dash state and clamp residual velocity
		// to walking speed so we don't sprint at 1400 cm/s into the floor.
		ResetDashState();
		Velocity = Velocity.GetClampedToMaxSize(MaxWalkSpeed);
	}

	if (bIsShadowNow && !bWasShadow)
	{
		// Entered shadow plane fresh — guarantee no leftover dash state and
		// clear any input direction cache so first frame doesn't double-up.
		ResetDashState();
		LastShadowInputDirection = FVector::ZeroVector;
	}
}

FVector UkdCharacterMovementComponent::GetCrushCollapseNormal() const
{
	if (const AkdMyPlayer* P = Cast<AkdMyPlayer>(GetCharacterOwner()))
	{
		return UkdCrushDirectionLibrary::MakeCrushBasis(P->GetActiveCrushDirection()).CollapseNormal;
	}
	return FVector(1.f, 0.f, 0.f); // safe default — behaves exactly like the old X-lock
}

void UkdCharacterMovementComponent::PhysShadow2D(float DeltaTime, int32 Iterations)
{
	//if (DeltaTime < MIN_TICK_TIME) return;

	//RestorePreAdditiveRootMotionVelocity();

	//// The locked axis for the current crush direction. (CHANGED: was implicit X.)
	//const FVector CollapseN = GetCrushCollapseNormal();

	//// Acceleration is populated by PerformMovement() from ConsumeInputVector() before
	//// any Phys* function runs — reading it here gives the full 2D shadow input (Y + Z).
	//// X is zeroed to keep the character locked to the shadow plane.
	////FVector ShadowAccel = FVector(0.f, Acceleration.Y, Acceleration.Z);

	//// Project input onto the play plane — drops the collapse-axis component,
	//// keeps the in-plane walk axis + vertical Z. (CHANGED: was FVector(0, Y, Z).)
	//FVector ShadowAccel = FVector::VectorPlaneProject(Acceleration, CollapseN);

	//const float AccelSize = ShadowAccel.Size();
	//const bool bHasInput = AccelSize > KINDA_SMALL_NUMBER;

	//if (bHasInput)
	//{
	//	// Cache direction for dash fallback — persists after input stops
	//	LastShadowInputDirection = ShadowAccel.GetSafeNormal();
	//	//LastShadowInputDirection.X = 0.0f;

	//	// Normalise then re-scale by our own acceleration value so diagonal
	//	// movement never exceeds the same speed as cardinal movement
	//	ShadowAccel = LastShadowInputDirection * ShadowAcceleration;
	//	Velocity += ShadowAccel * DeltaTime;
	//	//Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
	//	// /////////////////////////////////////////////////////////////////
	//	//if (!bIsDashing)
	//	//{
	//	//	Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
	//	//}
	//	//else if (Velocity.Size() <= ShadowMaxSpeed)
	//	//{
	//	//	// Dash has naturally decelerated to normal movement speed
	//	//	bIsDashing = false;
	//	//}
	//	//////////////////////////////////////////////////////////////////////
	//	if (bIsDashing)
	//	{
	//		// Decay current speed toward ShadowMaxSpeed at a rate proportional
	//		// to ShadowBrakingDeceleration. This makes the dash burst feel
	//		// like a momentum boost rather than a teleport.
	//		const float CurrentSpeed = Velocity.Size();
	//		if (CurrentSpeed > ShadowMaxSpeed)
	//		{
	//			const float DecayedSpeed = FMath::Max(
	//				ShadowMaxSpeed,
	//				CurrentSpeed - ShadowBrakingDeceleration * DeltaTime * 0.5f);
	//			Velocity = Velocity.GetSafeNormal() * DecayedSpeed;
	//		}
	//		else
	//		{
	//			bIsDashing = false;
	//			Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
	//		}
	//	}
	//	else
	//	{
	//		Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
	//	}
	//}
	//else
	//{
	//	// Linear braking: subtract a fixed cm/s per second from current speed.
	//	// More predictable than ApplyVelocityBraking() which relies on
	//	// BrakingDecelerationFlying defaulting to non-zero (it doesn't in UE).
	//	const float CurrentSpeed = Velocity.Size();
	//	if (CurrentSpeed > KINDA_SMALL_NUMBER)
	//	{
	//		const float BrakedSpeed = FMath::Max(0.f, CurrentSpeed - ShadowBrakingDeceleration * DeltaTime);
	//		Velocity = Velocity.GetSafeNormal() * BrakedSpeed;

	//		// Clear dash flag once the burst has decelerated to normal range
	//		if (BrakedSpeed <= ShadowMaxSpeed)
	//		{
	//			bIsDashing = false;
	//		}
	//	}
	//	else
	//	{
	//		Velocity = FVector::ZeroVector;
	//		bIsDashing = false;
	//	}
	//}

	//// Hard-zero X every frame — prevents drift from collisions or FP accumulation
	////Velocity.X = 0.f;

	//// Project velocity onto the play plane every frame — stops collapse-axis
	//// drift from collisions / FP error. (CHANGED: was Velocity.X = 0.)
	//Velocity = FVector::VectorPlaneProject(Velocity, CollapseN);

	//Iterations++;
	//FVector Delta = Velocity * DeltaTime;
	//FHitResult Hit(1.f);
	//SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	//if (Hit.IsValidBlockingHit())
	//{
	//	HandleImpact(Hit, DeltaTime, Delta);
	//	SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	//	Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
	//	Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
	//	//Velocity.X = 0.f;
	//	Velocity = FVector::VectorPlaneProject(Velocity, CollapseN); // (CHANGED: was Velocity.X = 0.)
	//	bIsDashing = false;
	//}


	if (DeltaTime < MIN_TICK_TIME) return;
 
	RestorePreAdditiveRootMotionVelocity();
 
	// The locked axis for the current crush direction.
	const FVector CollapseN = GetCrushCollapseNormal();
 
	// Acceleration is populated by PerformMovement() from ConsumeInputVector() before
	// any Phys* function runs — projecting it onto the play plane drops the collapse
	// axis and keeps the in-plane walk axis + vertical Z.
	FVector ShadowAccel = FVector::VectorPlaneProject(Acceleration, CollapseN);
 
	const float AccelSize = ShadowAccel.Size();
	const bool  bHasInput = AccelSize > KINDA_SMALL_NUMBER;
 
	// ─────────────────────────────────────────────────────────────────────────
	// Velocity resolution. Exactly ONE of these three branches runs per tick, so
	// dash and normal-input acceleration can never fight each other — that mutual
	// exclusivity is what makes the runaway-speed bug structurally impossible now.
	// ─────────────────────────────────────────────────────────────────────────
	if (bIsDashing)
	{
		// ── DASH STATE ────────────────────────────────────────────────────────
		// The dash owns velocity completely for its full duration. We tick the
		// timer down and drive speed off it — input CANNOT add energy here.
		DashTimeRemaining -= DeltaTime;
 
		// Ease the burst speed from its initial value back down to walk speed
		// across the whole window: a momentum boost that bleeds off, not a
		// teleport. Alpha runs 0 -> 1 as the dash progresses.
		const float Alpha = (DashDuration > KINDA_SMALL_NUMBER)
			? FMath::Clamp(1.f - (DashTimeRemaining / DashDuration), 0.f, 1.f)
			: 1.f;
		const float DashSpeed = FMath::Lerp(DashInitialSpeed, ShadowMaxSpeed, Alpha);
 
		// Direction is locked to the committed dash direction — a clean, readable
		// burst. (If you ever want in-dash steering, blend DashDirection toward
		// LastShadowInputDirection here by a small factor; leave it off for now.)
		Velocity = DashDirection * DashSpeed;
 
		// Cache steering intent so normal movement resumes in the right direction
		// the instant the dash ends.
		if (bHasInput)
		{
			LastShadowInputDirection = ShadowAccel.GetSafeNormal();
		}
 
		// Burst finished — drop back into normal movement, clamped to walk speed.
		if (DashTimeRemaining <= 0.f)
		{
			ResetDashState();
			Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
		}
	}
	else if (bHasInput)
	{
		// ── NORMAL MOVEMENT ───────────────────────────────────────────────────
		// Cache direction for the dash fallback, then accelerate and clamp.
		LastShadowInputDirection = ShadowAccel.GetSafeNormal();
 
		// Re-scale by our own acceleration so diagonal input never out-speeds
		// cardinal input.
		ShadowAccel = LastShadowInputDirection * ShadowAcceleration;
		Velocity += ShadowAccel * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(ShadowMaxSpeed);
	}
	else
	{
		// ── BRAKING ───────────────────────────────────────────────────────────
		// Linear braking: subtract a fixed cm/s per second from current speed.
		// More predictable than ApplyVelocityBraking(), which relies on
		// BrakingDecelerationFlying (zero by default in UE).
		const float CurrentSpeed = Velocity.Size();
		if (CurrentSpeed > KINDA_SMALL_NUMBER)
		{
			const float BrakedSpeed = FMath::Max(0.f, CurrentSpeed - ShadowBrakingDeceleration * DeltaTime);
			Velocity = Velocity.GetSafeNormal() * BrakedSpeed;
		}
		else
		{
			Velocity = FVector::ZeroVector;
		}
	}
 
	// Project velocity onto the play plane every frame — stops collapse-axis
	// drift from collisions / FP error.
	Velocity = FVector::VectorPlaneProject(Velocity, CollapseN);
 
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
		Velocity = FVector::VectorPlaneProject(Velocity, CollapseN);
 
		// Hitting a wall aborts the dash — no point pushing a locked burst into geometry.
		ResetDashState();
	}
}
