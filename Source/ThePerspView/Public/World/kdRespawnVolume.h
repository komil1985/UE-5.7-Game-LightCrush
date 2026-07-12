// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdRespawnVolume.generated.h"

class UBoxComponent;
class AkdMyPlayer;

/**
 * AkdRespawnVolume — the fall-recovery plane.
 *
 * A large, flat box placed BELOW every walkable surface. When a fallen player
 * overlaps it, they are teleported to the last checkpoint recorded by
 * UkdTutorialSubsystem (i.e. the last AkdTutorialTrigger they touched), with
 * their momentum zeroed so they don't inherit the fall.
 *
 * OWNERSHIP: this actor holds NO checkpoint state of its own. It only reads the
 * shared checkpoint from the subsystem — one source of truth, same discipline as
 * the rest of the tutorial system.
 *
 * SETUP:
 *   1. Drop one in the level. Scale TriggerBox to span the whole playable area.
 *   2. Position it comfortably below the lowest floor.
 *   3. That's it — checkpoints are driven entirely by the tutorial triggers.
 */
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdRespawnVolume : public AActor
{
    GENERATED_BODY()

public:
    AkdRespawnVolume();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    /** Lifts the recovered player this far above the checkpoint (anti-embed). */
    UPROPERTY(EditAnywhere, Category = "Respawn", meta = (ClampMin = "0.0"))
    float RespawnVerticalOffset = 20.f;

    /** Debounce so a single fall can't fire the teleport on multiple sub-steps. */
    UPROPERTY(EditAnywhere, Category = "Respawn", meta = (ClampMin = "0.0"))
    float RespawnCooldown = 0.3f;

private:
    UFUNCTION()
    void OnBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& Sweep);

    void RespawnPlayer(AkdMyPlayer* Player);

    /** Real time (not dilated) of the last teleport — see cpp for why. */
    float LastRespawnTime = -1000.f;
};