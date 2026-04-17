// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdLevelGoal.generated.h"


class USphereComponent;
class UStaticMeshComponent;
class UParticleSystemComponent;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdLevelGoal : public AActor
{
	GENERATED_BODY()
	
public:
    AkdLevelGoal();
    virtual void BeginPlay() override;

    /** If true the player must be in Crush Mode (State.CrushMode) to trigger. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal")
    bool bRequireCrushMode = true;

    /** If true the player must also be standing in shadow (State.InShadow). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal")
    bool bRequireInShadow = false;

    /** Blueprint hook for win VFX / sound. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Goal")
    void BP_OnGoalReached();

protected:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> GoalMesh;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USphereComponent> TriggerSphere;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UParticleSystemComponent> IdleParticle;

private:
    bool bTriggered = false;

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                 OtherBodyIndex,
        bool                  bFromSweep,
        const FHitResult& SweepResult);

};


// ─────────────────────────────────────────────────────────────────────────────
// AkdLevelGoal — the level exit trigger.
//
// Design intent for "The Persp View":
//   The exit portal is only reachable from the shadow plane, so it sits flat
//   against the wall surface where shadow enemies patrol.  The player must
//   navigate to it in Crush Mode while managing stamina.
//
// HOW IT WORKS:
//   • Player overlaps the TriggerSphere.
//   • If the player is in Crush Mode (or any mode if bRequireCrushMode = false),
//     GameMode->TriggerLevelComplete() is called once.
//   • Fires BP_OnGoalReached for Blueprint VFX / sound authoring.
//
// SETUP:
//   1. Drop a BP subclass into the level at the exit location.
//   2. Enable bRequireCrushMode (default true) so only shadow-plane players
//      can finish — prevents accidentally completing the level in 3D.
// ─────────────────────────────────────────────────────────────────────────────