// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdFloorBase.generated.h"

class UStaticMeshComponent;
UCLASS()
class THEPERSPVIEW_API AkdFloorBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AkdFloorBase();
	
	UPROPERTY(EditDefaultsOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	FVector OriginalFloorScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	FVector OriginalFloorLocation;

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

};
