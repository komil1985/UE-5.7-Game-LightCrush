// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdScorePickup.generated.h"


class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class URotatingMovementComponent;
class UkdHoverBobComponent;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdScorePickup : public AActor
{
	GENERATED_BODY()
	
public:
    AkdScorePickup();
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    int32 ScoreValue = 100;

    /** Stamina points restored on pickup (0 = score-only pickup). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    float StaminaRestore = 15.f;

    /** If true, this pickup can only be collected while the player is in Crush Mode.
    *  Default false — collectible in both 3D and Crush (2D) mode. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    bool bRequireCrushMode = false;

    UPROPERTY(EditDefaultsOnly, Category = "Pickup")
    TObjectPtr<USoundBase> PickupSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Effects", meta = (ClampMin = "0.1"))
    float CollectBurstScale = 1.f;

    /** Blueprint hook for visual/audio feedback on collection. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
    void BP_OnCollected();

protected:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> PickupMesh;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USphereComponent> PickupSphere;

    UPROPERTY(EditDefaultsOnly, Category = "Pickup|Effects")
    TObjectPtr<UNiagaraSystem> CollectBurstNiagara;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UNiagaraComponent> PickupEffect;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<URotatingMovementComponent> RotatingMovement;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UkdHoverBobComponent> HoverBob;

private:
    bool bCollected = false;

    UFUNCTION()
    void OnPickupSphereOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                 OtherBodyIndex,
        bool                  bFromSweep,
        const FHitResult& SweepResult);

};


// ─────────────────────────────────────────────────────────────────────────────
// AkdScorePickup — a collectible crystal that awards points and optionally
// restores stamina.  Only collectible while in Crush Mode.
//
// SETUP:
//   1. Place BP_ScorePickup actors throughout the shadow plane.
//   2. Tune ScoreValue and StaminaRestore per placement context.
//   3. Assign PickupSound and PickupParticle in the Blueprint defaults.
// ─────────────────────────────────────────────────────────────────────────────