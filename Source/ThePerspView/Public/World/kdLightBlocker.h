// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/kdInteractable.h"
#include "kdLightBlocker.generated.h"

class USphereComponent;
class UStaticMeshComponent;
UCLASS()
class THEPERSPVIEW_API AkdLightBlocker : public AActor, public IkdInteractable
{
	GENERATED_BODY()
	
public:	
	AkdLightBlocker();

	virtual void Tick(float DeltaTime) override;

	// IkdInteractable
	virtual void Interact(class AkdMyPlayer* InInstigator) override;

	// ── Mesh ─────────────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> BlockerMesh;

	// ── Interaction ───────────────────────────────────────────────────────────

	/** Radius within which the player can trigger an interaction (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	TObjectPtr<USphereComponent> InteractionSphere;

	// ── Movement ──────────────────────────────────────────────────────────────

	/**
	 * Resting position A (default: actor's placement location in the level).
	 * Set to world-space coordinates in the Details panel.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Movement")
	FVector PositionA = FVector::ZeroVector;

	/**
	 * Resting position B — where the blocker moves to when toggled.
	 * Set to world-space coordinates in the Details panel.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Movement")
	FVector PositionB = FVector::ZeroVector;

	/** How fast the blocker slides between positions (fraction of distance per second). */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float LerpSpeed = 2.f;

protected:
	virtual void BeginPlay() override;

private:
	float Alpha = 0.f;   // 0 = at PositionA, 1 = at PositionB
	bool  bMovingToB = false;
	bool  bIsMoving = false;

};
