// Copyright ASKD Games


#include "World/kdLightBlocker.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"

AkdLightBlocker::AkdLightBlocker()
{
	PrimaryActorTick.bCanEverTick = true;

	BlockerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockerMesh"));
	SetRootComponent(BlockerMesh);
	// The mesh must use an opaque material so the directional light shadow is cast properly.
	// Translucent or masked materials will not block the line trace.

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(200.f);
	// Collision: overlap-only; no physics blocking needed for the interaction sphere
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAll"));
	InteractionSphere->SetGenerateOverlapEvents(true);

}

void AkdLightBlocker::BeginPlay()
{
	Super::BeginPlay();

	// PositionA defaults to where the designer placed the actor in the level.
	// The designer should override both via the Details panel if they want
	// the blocker to start somewhere else, but this is a sensible default.
	if (PositionA.IsNearlyZero())
	{
		PositionA = GetActorLocation();
	}
	if (PositionB.IsNearlyZero())
	{
		// Offset B by 500cm along Y so there's a visible default movement
		PositionB = PositionA + FVector(0.f, 500.f, 0.f);
	}
	
}

void AkdLightBlocker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsMoving) return;

	// Advance Alpha toward 0 or 1 depending on which position is the target
	const float Direction = bMovingToB ? 1.f : -1.f;
	Alpha = FMath::Clamp(Alpha + Direction * LerpSpeed * DeltaTime, 0.f, 1.f);

	// Smooth lerp between the two world positions
	SetActorLocation(FMath::Lerp(PositionA, PositionB, Alpha));

	// Stop ticking once we've arrived
	if (FMath::IsNearlyEqual(Alpha, bMovingToB ? 1.f : 0.f))
	{
		bIsMoving = false;
	}
}

void AkdLightBlocker::Interact(AkdMyPlayer* InInstigator)
{
	if (!InInstigator) return;

	// Gate interaction to Crush Mode only — matches the Interact_CrushOnly tag intent
	UAbilitySystemComponent* ASC = InInstigator->GetAbilitySystemComponent();
	if (!ASC || !ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("LightBlocker: Interact rejected — player not in Crush Mode"));
#endif
		return;
	}

	// Toggle target and start sliding
	bMovingToB = !bMovingToB;
	bIsMoving = true;

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("LightBlocker: Moving to %s"), bMovingToB ? TEXT("B") : TEXT("A"));
#endif
}

