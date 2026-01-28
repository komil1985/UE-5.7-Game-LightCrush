// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "kdMyPlayer.generated.h"

USTRUCT()
struct FTransformData
{
	GENERATED_BODY()
	
	FVector PlayerStartLocation;
	FVector PlayerTargetLocation;
	FRotator SpringArmStartRotation;
	FRotator SpringArmTargetRotation;
	float SpringArmStartLength;
	float SpringArmTargetLength;

	TArray<FVector> FloorStartScales;
	TArray<FVector> FloorTargetScales;
	TArray<FVector> FloorStartLocations;
	TArray<FVector> FloorTargetLocations;
};


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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crush Mechanic")
	FVector OriginalPlayerLocation;		// Store original player location for Restore Mode

	UPROPERTY()
	TArray<TObjectPtr<AkdFloorBase>>FloorActors;	// References to floor actors in the world

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crush Mechanic")
	bool bIsCrushMode = false;	// Crush mode state flag

	UPROPERTY()
	TObjectPtr<AkdFloorBase> CurrentFloorActor;	// Current floor actor being interacted with

	UPROPERTY()
	TMap<TObjectPtr<AkdFloorBase>, FVector> PlayerRelativePositionsPerFloor; // Store player relative positions before floor interaction

	UFUNCTION(BlueprintCallable, Category = "Crush Mechanic")
	void ToggleCrushMode();		// Toggle between Crush Mode and Restore Mode

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void FindFloorActors(UWorld* World);	// Find and store references to floor actors

	UFUNCTION()
	void UpdateCurrentFloor();

	UFUNCTION()
	void CachePlayerRelativePosition();

	// Transition variables //
	UPROPERTY()
	bool bIsTranstioning = false;

	UPROPERTY()
	float TransitionAlpha = 0.0f;

	UPROPERTY()
	float TransitionDuration = 0.7f;  // Duration of the transition in seconds

	UPROPERTY()
	bool bTargetCrushMode = false;

	UPROPERTY()
	FTransformData TransitionData; // Data for smooth transitions

	UFUNCTION()
	void CrushTransition();

	UFUNCTION()
	void CrushInterpolation(float DeltaTime);
};
