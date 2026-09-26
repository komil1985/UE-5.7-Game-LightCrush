// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "kdBuoyancyRippleSubsystem.generated.h"

class UkdBuoyancyComponent;

/**
 * UkdBuoyancyRippleSubsystem
 *
 * Lightweight per-world registry that lets a UkdBuoyancyComponent pass a
 * fraction of its landing impulse to nearby buoyancy platforms, so a
 * connected "raft" of floating platforms shudders faintly when one of them
 * takes a hit — without any single component holding a direct reference to
 * its neighbors. Mirrors UkdMenuPresentationSubsystem's registry pattern.
 *
 * Opt-in per platform: RippleRadius = 0 (the default on UkdBuoyancyComponent)
 * means a platform never emits a ripple, though it still registers so OTHER
 * platforms can reach it.
 */
UCLASS()
class THEPERSPVIEW_API UkdBuoyancyRippleSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    void RegisterPlatform(UkdBuoyancyComponent* Platform);
    void UnregisterPlatform(UkdBuoyancyComponent* Platform);

    /** Called by a platform the instant it takes a landing impulse. Passes a
     *  falloff-scaled fraction of VelocityKick to every OTHER registered
     *  platform within Radius (linear falloff: full strength at distance 0,
     *  zero at Radius). */
    void BroadcastImpulse(const UkdBuoyancyComponent* Source, const FVector& WorldLocation,
        float VelocityKick, float Radius, float Strength) const;

private:
    UPROPERTY()
    TArray<TObjectPtr<UkdBuoyancyComponent>> RegisteredPlatforms;
};