// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "kdMyPlayer.generated.h"


class UCameraComponent;
class USpringArmComponent;
class AkdFloorBase;
UCLASS()
class THEPERSPVIEW_API AkdMyPlayer : public ACharacter
{
	GENERATED_BODY()

public:
	AkdMyPlayer();
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanic")
	FVector OriginalPlayerLocation;		// Store original player location for Restore Mode

	UPROPERTY()
	TArray<TObjectPtr<AkdFloorBase>>FloorActors;	// References to floor actors in the world

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanic")
	bool bIsCrushMode = false;	// Crush mode state flag

	UPROPERTY()
	TObjectPtr<AkdFloorBase> CurrentFloorActor;	// Current floor actor being interacted with

	UPROPERTY()
	TMap<TObjectPtr<AkdFloorBase>, FVector> PlayerRelativePositionsPerFloor; // Store player relative positions before floor interaction

	UFUNCTION(BlueprintCallable, Category = "Mechanic")
	void ToggleCrushMode();		// Toggle between Crush Mode and Restore Mode

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void FindFloorActors(UWorld* World);	// Find and store references to floor actors

	void UpdateCurrentFloor();
	void CachePlayerRelativePosition();
};
