// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdTutorialTrigger.generated.h"

class UBoxComponent;

/**
 * AkdTutorialTrigger — a volume that arms a lesson.
 *
 * It knows nothing about widgets, tags, or timing. It only says:
 *   "the player is here; StepId is now relevant."
 *
 * SETUP:
 *   1. Drop in the level, scale TriggerBox to the teaching space.
 *   2. Set StepId to match an entry in DA_TutorialBank.
 *   3. For ExitVolume steps, leave bOneShotPerLevel = true and let the exit finish it.
 */
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdTutorialTrigger : public AActor
{
    GENERATED_BODY()

public:
    AkdTutorialTrigger();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    /** Must match a StepId in DA_TutorialBank. */
    UPROPERTY(EditInstanceOnly, Category = "Tutorial")
    FName StepId = NAME_None;

    /** Fire on the first entry only. Off = re-arms every entry. */
    UPROPERTY(EditInstanceOnly, Category = "Tutorial")
    bool bOneShotPerLevel = true;

    /** Leaving before learning yanks the lesson (use for optional side-rooms). */
    UPROPERTY(EditInstanceOnly, Category = "Tutorial")
    bool bCancelOnExit = false;

private:
    UFUNCTION()
    void OnBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

    UFUNCTION()
    void OnEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    bool bFired = false;
};