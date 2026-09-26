// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdFloorBase.generated.h"

class UStaticMeshComponent;
class UkdBuoyancyComponent;
UCLASS()
class THEPERSPVIEW_API AkdFloorBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AkdFloorBase();

	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	FVector OriginalFloorScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	FVector OriginalFloorLocation;

protected:
	virtual void BeginPlay() override;	

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UkdBuoyancyComponent> BuoyancyComponent;
};
