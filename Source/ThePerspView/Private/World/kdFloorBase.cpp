// Copyright ASKD Games


#include "World/kdFloorBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/kdBuoyancyComponent.h"


AkdFloorBase::AkdFloorBase()
{
	PrimaryActorTick.bCanEverTick = false;

	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	SetRootComponent(FloorMesh);

	BuoyancyComponent = CreateDefaultSubobject<UkdBuoyancyComponent>(TEXT("BuoyancyComponent"));
}

void AkdFloorBase::BeginPlay()
{
	Super::BeginPlay();
	
	if (FloorMesh)
	{
		OriginalFloorScale = FloorMesh->GetComponentScale();
	}
	OriginalFloorLocation = GetActorLocation();
}

void AkdFloorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
