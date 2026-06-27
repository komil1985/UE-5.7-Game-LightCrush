// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdLevelGoal.generated.h"


class USphereComponent;
class UStaticMeshComponent;
class UParticleSystemComponent;
class UAbilitySystemComponent;
class AkdMyPlayer;
struct FGameplayTag;

/**
 * Which world-state plane the player must occupy for this goal to accept them.
 * Authored per-goal, so one level can mix 2D-only exits, 3D-only exits, and
 * exits reachable from either plane.
 */
UENUM(BlueprintType)
enum class EkdGoalReachMode : uint8
{
    /** Completes only while in Crush Mode (2D shadow plane). */
    CrushModeOnly UMETA(DisplayName = "Crush Mode Only (2D)"),

    /** Completes only while in Light Mode (3D world). */
    LightModeOnly UMETA(DisplayName = "Light Mode Only (3D)"),

    /** Completes from either plane — 2D or 3D. */
    EitherMode    UMETA(DisplayName = "Either Mode (2D or 3D)")
};


UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdLevelGoal : public AActor
{
	GENERATED_BODY()
	
public:
    AkdLevelGoal();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Which plane(s) the player may complete this goal from. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal")
    EkdGoalReachMode ReachMode = EkdGoalReachMode::EitherMode;

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

    /** Player currently inside the trigger, if any. Weak — never owns. */
    TWeakObjectPtr<AkdMyPlayer> TrackedPlayer;

    /** ASC we registered tag callbacks on, kept so we can cleanly unregister. */
    TWeakObjectPtr<UAbilitySystemComponent> TrackedASC;

    FDelegateHandle CrushTagHandle;
    FDelegateHandle ShadowTagHandle;

    /** True if the given ASC satisfies ReachMode + the InShadow sub-condition. */
    bool IsPlayerEligible(UAbilitySystemComponent* ASC) const;

    /** Evaluates the tracked player and completes the level if eligible. Idempotent. */
    void TryComplete();

    /** Subscribes to the tracked player's relevant state-tag changes. */
    void BindStateTracking(AkdMyPlayer* Player);

    /** Clears any active state-tag subscription. */
    void UnbindStateTracking();

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                 OtherBodyIndex,
        bool                  bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnTriggerEndOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex);

    /**
     * Fired when State.CrushMode / State.InShadow flips while the player is
     * still inside the trigger — lets the goal complete the instant the player
     * crushes (or steps into shadow) while standing on the exit, instead of
     * requiring a fresh BeginOverlap.
     */
    void OnTrackedStateChanged(const FGameplayTag ChangedTag, int32 NewCount);

};


// ─────────────────────────────────────────────────────────────────────────────
// AkdLevelGoal — the level exit trigger.
//
// HOW IT WORKS:
//   • Player overlaps TriggerSphere.
//   • IsPlayerEligible() checks ReachMode (2D-only / 3D-only / either) plus the
//     optional InShadow sub-condition.
//   • If eligible, GameMode->TriggerLevelComplete() fires exactly once and
//     BP_OnGoalReached() runs for VFX / sound.
//   • If the player overlaps but isn't yet eligible (e.g. a Crush-only goal
//     entered in 3D), the goal listens to their CrushMode / InShadow tags and
//     completes the moment they satisfy the requirement on the spot.
//
// SETUP:
//   1. Drop a BP subclass at the exit location.
//   2. Pick ReachMode: EitherMode (default) finishes in 2D or 3D; CrushModeOnly
//      locks the exit to the shadow plane; LightModeOnly to the 3D world.
// ─────────────────────────────────────────────────────────────────────────────