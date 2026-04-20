// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdKillVolume.generated.h"


class UBoxComponent;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdKillVolume : public AActor
{
	GENERATED_BODY()
	
public:	
	AkdKillVolume();

#if WITH_EDITOR
	// Draw a visible box in the editor so designers can see the volume
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> KillBox;

private:
	UFUNCTION()
	void OnKillBoxOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32                 OtherBodyIndex,
		bool                  bFromSweep,
		const FHitResult& SweepResult);

};
