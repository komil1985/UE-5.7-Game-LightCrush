// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdCheckpoint.generated.h"


class USphereComponent;
class UStaticMeshComponent;
class UParticleSystemComponent;
class AkdGameModeBase;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdCheckpoint : public AActor
{
	GENERATED_BODY()
	
public:
    AkdCheckpoint();

    virtual void BeginPlay() override;

    /** Returns the transform the player should be teleported to on respawn. */
    UFUNCTION(BlueprintPure, Category = "Checkpoint")
    FTransform GetRespawnTransform() const;

    UFUNCTION(BlueprintPure, Category = "Checkpoint")
    bool IsActivated() const { return bActivated; }

protected:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> CheckpointMesh;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USphereComponent> TriggerSphere;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UParticleSystemComponent> ActivationParticle;

    // ── Config ────────────────────────────────────────────────────────────────

    /** World-space Z offset added to this actor's location for the spawn point. */
    UPROPERTY(EditAnywhere, Category = "Checkpoint")
    float RespawnOffsetZ = 90.f;

    /** Material applied to CheckpointMesh when activated. */
    UPROPERTY(EditDefaultsOnly, Category = "Checkpoint")
    TObjectPtr<UMaterialInterface> ActiveMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Checkpoint")
    TObjectPtr<USoundBase> ActivationSound;

private:
    bool bActivated = false;

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    void Activate();

};


// ─────────────────────────────────────────────────────────────────────────────
// AkdCheckpoint — touch-to-activate respawn anchor.
//
// HOW IT WORKS:
//   • Player walks into the TriggerSphere → NotifyGameMode registers this as
//     the active checkpoint for AkdGameModeBase.
//   • The mesh / particle state changes from "inactive" to "active" so the
//     player can see which checkpoint they last touched.
//   • Only the player pawn triggers activation (tag-gated to avoid enemies
//     accidentally resetting the checkpoint).
//
// SETUP:
//   1. Drop a BP subclass into the level.
//   2. Assign a glowing "active" material to ActiveMaterial in the Details panel.
//   3. Optionally assign an ActivationParticle system for a pop effect.
//   4. Set RespawnOffsetZ so the player respawns standing on the floor,
//      not inside the ground (default 90 cm accounts for capsule half-height).
// ─────────────────────────────────────────────────────────────────────────────