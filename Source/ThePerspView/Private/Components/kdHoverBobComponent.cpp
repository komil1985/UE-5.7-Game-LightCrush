// Copyright ASKD Games

#include "Components/kdHoverBobComponent.h"
#include "GameFramework/Actor.h"


UkdHoverBobComponent::UkdHoverBobComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    bAutoActivate = true;
}

void UkdHoverBobComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!TargetComponent && GetOwner())
    {
        TargetComponent = GetOwner()->GetRootComponent();
    }

    if (TargetComponent)
    {
        BaseRelativeLocation = TargetComponent->GetRelativeLocation();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("UkdHoverBobComponent on %s: no TargetComponent and owner has no root — disabling."),
            GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
        SetComponentTickEnabled(false);
    }

    PhaseOffset = bRandomizePhase ? FMath::FRandRange(0.f, 2.f * PI) : 0.f;
}

void UkdHoverBobComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!TargetComponent) return;

    const float TimeSeconds = GetWorld()->GetTimeSeconds();
    const float ZOffset = Amplitude * FMath::Sin((TimeSeconds * Frequency * 2.f * PI) + PhaseOffset);

    FVector NewRelativeLocation = BaseRelativeLocation;
    NewRelativeLocation.Z += ZOffset;
    TargetComponent->SetRelativeLocation(NewRelativeLocation);
}