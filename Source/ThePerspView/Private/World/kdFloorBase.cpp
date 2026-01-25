// Copyright ASKD Games


#include "World/kdFloorBase.h"
#include "Components/StaticMeshComponent.h"

AkdFloorBase::AkdFloorBase()
{
	PrimaryActorTick.bCanEverTick = false;

	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	FloorMesh->SetupAttachment(RootComponent);
}

void AkdFloorBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void AkdFloorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}