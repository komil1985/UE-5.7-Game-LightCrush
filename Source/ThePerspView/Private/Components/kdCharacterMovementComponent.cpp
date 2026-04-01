// Copyright ASKD Games


#include "Components/kdCharacterMovementComponent.h"
#include "Player/kdMyPlayer.h"


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

    // Get input
    FVector Input = ConsumeInputVector();

    // Only allow Y (horizontal 2D) + Z (vertical shadow movement)
    FVector ShadowInput = FVector(0.f, Input.Y, Input.Z);

    // Normalize to avoid faster diagonal movement
    ShadowInput = ShadowInput.GetClampedToMaxSize(1.0f);

    // Apply acceleration
    FVector acceleration = ShadowInput * MaxAcceleration;

    // Update velocity
    Velocity += acceleration * DeltaTime;

    // Clamp speed
    Velocity = Velocity.GetClampedToMaxSize(MaxFlySpeed);

    // Apply braking when no input
    if (ShadowInput.IsNearlyZero())
    {
        ApplyVelocityBraking(DeltaTime, BrakingDecelerationFlying, BrakingFriction);
    }

    // Move character
    FVector Delta = Velocity * DeltaTime;
    FHitResult Hit;
    SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

    // Handle collisions
    if (Hit.IsValidBlockingHit())
    {
        //SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
        HandleImpact(Hit, DeltaTime, Delta);
        SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
        // Kill velocity component pressing into the surface to avoid sticking
        Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
        Velocity = Velocity.GetClampedToMaxSize(MaxFlySpeed);
    }
}
