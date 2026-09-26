// Copyright ASKD Games


#include "Subsystem/kdBuoyancyRippleSubsystem.h"
#include "Components/kdBuoyancyComponent.h"

void UkdBuoyancyRippleSubsystem::RegisterPlatform(UkdBuoyancyComponent* Platform)
{
    if (Platform)
    {
        RegisteredPlatforms.AddUnique(Platform);
    }
}

void UkdBuoyancyRippleSubsystem::UnregisterPlatform(UkdBuoyancyComponent* Platform)
{
    RegisteredPlatforms.RemoveSingleSwap(Platform);
}

void UkdBuoyancyRippleSubsystem::BroadcastImpulse(const UkdBuoyancyComponent* Source, const FVector& WorldLocation,
    float VelocityKick, float Radius, float Strength) const
{
    if (Radius <= KINDA_SMALL_NUMBER || Strength <= KINDA_SMALL_NUMBER) return;

    for (UkdBuoyancyComponent* Platform : RegisteredPlatforms)
    {
        if (!IsValid(Platform) || Platform == Source) continue;

        const float Distance = FVector::Dist(WorldLocation, Platform->GetMeshWorldLocation());
        if (Distance >= Radius) continue;

        const float Falloff = 1.f - (Distance / Radius);   // 1 at source -> 0 at Radius
        Platform->ReceiveRippleImpulse(VelocityKick * Strength * Falloff);
    }
}
