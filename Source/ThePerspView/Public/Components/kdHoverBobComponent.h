// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdHoverBobComponent.generated.h"

class USceneComponent;

/**
 * Drop-on ambient float/hover animation. Offsets TargetComponent (defaults to
 * the owner's root) up and down on a sine wave. Purely cosmetic — carries no
 * gameplay state, safe to attach to any actor (pickups, signage, etc).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdHoverBobComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UkdHoverBobComponent();

    /** Component to animate. Leave unset to animate the owner's root component. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover")
    TObjectPtr<USceneComponent> TargetComponent;

    /** Peak vertical offset in cm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover", meta = (ClampMin = "0"))
    float Amplitude = 15.f;

    /** Bob speed in cycles per second. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover", meta = (ClampMin = "0"))
    float Frequency = 0.75f;

    /** Randomizes the starting phase so multiple instances don't bob in lockstep. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover")
    bool bRandomizePhase = true;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    FVector BaseRelativeLocation = FVector::ZeroVector;
    float PhaseOffset = 0.f;
};