// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdShadowPortal.generated.h"

class USphereComponent;
class UStaticMeshComponent;
UCLASS()
class THEPERSPVIEW_API AkdShadowPortal : public AActor
{
	GENERATED_BODY()
	
public:	
	AkdShadowPortal();

	// ── Visual ────────────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	// ── Trigger ───────────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	TObjectPtr<USphereComponent> TriggerSphere;

	// ── Configuration ─────────────────────────────────────────────────────────

	/** The other portal this one teleports the player to. Set in the editor. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Portal")
	TObjectPtr<AkdShadowPortal> LinkedPortal;

	/**
	 * Local-space offset applied at the exit portal so the player doesn't
	 * spawn inside the destination trigger sphere. Default pushes forward (+Y).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	FVector ExitOffset = FVector(0.f, 0.f, 175.f);

	/** Seconds both portals are disabled after a teleport. */
	UPROPERTY(EditDefaultsOnly, Category = "Portal", meta = (ClampMin = "0.1"))
	float CooldownDuration = 1.5f;

	// ── State (readable by linked portal) ────────────────────────────────────

	bool bCanTeleport = true;

protected:
	virtual void BeginPlay() override;

private:
	// ── Crush Mode Visibility ─────────────────────────────────────────────────

	/**
	 * Registered on the player's ASC in BeginPlay.
	 * Shows/hides the portal whenever State.CrushMode is added or removed.
	 */
	UFUNCTION()
	void OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	/** Shows mesh + enables trigger. Called when CrushMode is active. */
	void SetPortalActive(bool bActive);

	// ── Teleport ──────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnTriggerBeginOverlap (UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void EnableTeleport();

	FTimerHandle CooldownTimerHandle;

};


// AkdShadowPortal — a one-way teleporter that only works while the player
// is in Crush Mode AND standing in shadow (State.InShadow).
//
// SETUP (editor):
//   1. Place two AkdShadowPortal actors in your level.
//   2. On each one, set LinkedPortal to point to the other.
//   3. Adjust ExitOffset on each portal so the player doesn't spawn
//      inside the trigger volume of the destination portal.
//
// Both portals go on cooldown simultaneously after a teleport, preventing
// the player from immediately bouncing back.