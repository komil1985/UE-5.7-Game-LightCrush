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
	FVector PlayerOriginalScale;
	FVector PlayerTargetScale;
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
class UCameraShakeBase;
class USoundBase;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crush Mechanic")
	FVector OriginalPlayerScale;		// Store original player scale for Restore Mode

	UPROPERTY()
	TArray<TObjectPtr<AkdFloorBase>>FloorActors;	// References to floor actors in the world

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crush Mechanic")
	bool bIsCrushMode = false;	// Crush mode state flag

	UPROPERTY(EditAnywhere, Category = "Crush Effects")
	TSubclassOf<UCameraShakeBase> CrushCameraShake;	// Camera shake class for crush/restore transition

	UPROPERTY(EditAnywhere, Category = "Crush Effects")
	TObjectPtr<USoundBase> CrushSound;	// Sound played during crush/restore transition

	UPROPERTY(EditAnywhere, Category = "Crush Effects")
	TObjectPtr<USoundBase> ToggleCrushSound;	// Sound played when toggling crush mode

	UPROPERTY()
	TObjectPtr<AkdFloorBase> CurrentFloorActor;	// Current floor actor being interacted with

	UPROPERTY()
	TObjectPtr<AkdFloorBase> LastCachedFloor = nullptr;  // Last floor for which player relative position was cached

	UPROPERTY()
	TMap<TObjectPtr<AkdFloorBase>, FVector> PlayerRelativePositionsPerFloor; // Store player relative positions before floor interaction

	UFUNCTION(BlueprintCallable, Category = "Crush Mechanic")
	void ToggleCrushMode();		// Toggle between Crush Mode and Restore Mode

	UFUNCTION(BlueprintCallable, Category = "Crush Mechanic")
	void MoveUpInShadow(float Value);		// Move player up while in shadow (Crush Mode)

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void FindFloorActors(UWorld* World);	// Find and store references to floor actors

	UFUNCTION()
	void UpdateCurrentFloor();	// Update the current floor actor based on player position

	UFUNCTION()
	void CachePlayerRelativePosition();	// Cache player relative position to the current floor

	// Transition variables //
	UPROPERTY()
	bool bIsTransitioning = false;	// Flag to indicate if a transition is in progress

	UPROPERTY()
	float TransitionAlpha = 0.0f;	// Alpha value for interpolation during transition

	UPROPERTY(EditDefaultsOnly, Category = "Crush Mechanic")
	float TransitionDuration = 0.4f;  // Duration of the transition in seconds

	UPROPERTY()
	bool bTargetCrushMode = false;	// Target mode for the transition

	UPROPERTY()
	FTransformData TransitionData; // Data for smooth transitions

	UFUNCTION()
	void CrushTransition();	// Start the crush/restore transition

	UFUNCTION()
	void CrushInterpolation(float DeltaTime);	// Handle interpolation during transition

	UFUNCTION()
	bool IsStandingInShadow();	// Check if player is standing in shadow

	UPROPERTY()
	AActor* DirectionalLightActor = nullptr;	// Reference to the directional light actor

	UPROPERTY()
	FVector CachedLightDirection = FVector::ZeroVector;	// Cached light direction for shadow calculations

	UPROPERTY(EditAnywhere, Category = "Crush Mechanic")
	bool bShowShadowDebugLines = true;		// Toggle for debug lines

private:
	float PlayerCrushScale = 0.001f;	// Scale factor for player crush effect
	float FloorCrushScale = 0.001f;		// Scale factor for floor crush effect

	bool bProjectionSwitched = false; // Flag to track if projection has been switched


	UPROPERTY(EditAnywhere, Category = "Crush Visuals")
	UMaterialInterface* CrushPostProcessMaterial; // Assign M_CrushOutline here in Blueprint

	// Dynamic instance so we can fade it in/out
	UPROPERTY()
	UMaterialInstanceDynamic* CrushPPInstance;

	// Weighted blendable to control intensity
	FWeightedBlendable CrushBlendable;
};
