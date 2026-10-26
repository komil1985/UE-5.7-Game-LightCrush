// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "kdLevelIntroSubsystem.generated.h"

class ACameraActor;
class APlayerController;
class AkdMyPlayer;
class AkdLevelGoal;

/**
 * UkdLevelIntroSubsystem — the "where do I need to go?" shot.
 *
 * At the start of a level the view is placed on the AkdLevelGoal, holds for a
 * beat, then blends smoothly back to the player's own camera. Runs ONCE per
 * world: death/respawn teleports the same pawn and never reloads the world, so
 * the subsystem (and its "already played" state) simply persists.
 *
 * OWNERSHIP CONTRACT
 *   • Never writes the player's SpringArm / Camera (owned by CrushTransition /
 *     StrategicCamera). It spawns a transient ACameraActor and uses the engine's
 *     view-target blend, which re-evaluates the pawn's live POV every frame.
 *   • Blocks gameplay input via AkdPlayerController::SetGameplayInputBlocked.
 *   • Suspends the level timer via AkdGameModeBase::SetLevelTimerSuspended.
 *   • Edge pulse (optional) is routed through UkdWorldColorDriver.
 *
 * PHASES
 *   WaitingForPawn -> Holding -> Blending -> Finished   (Disabled if skipped)
 */
UCLASS()
class THEPERSPVIEW_API UkdLevelIntroSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    // ── FTickableGameObject ───────────────────────────────────────────────────
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;

    static UkdLevelIntroSubsystem* Get(const UObject* WorldContext);

    /** Called by AkdPlayerController on any key press while input is blocked.
     *  Ends the hold early and flies home with SkipBlendSeconds. */
    void RequestSkip();

    /** True while the intro owns the view. */
    bool IsIntroActive() const;

private:
    enum class EPhase : uint8 { Disabled, WaitingForPawn, Holding, Blending, Finished };

    void TryBegin();
    AkdLevelGoal* FindGoal() const;
    void BeginBlend(float BlendSeconds);
    bool IsIntroStillValid() const;

    /** bAborted = true snaps the view back to the pawn instantly (death, level
     *  complete). false = normal finish, the blend already landed on the pawn. */
    void EndIntro(bool bAborted);
    void ReleaseIntroResources();

    EPhase Phase = EPhase::Disabled;
    float  PhaseTime = 0.f;
    float  ActiveBlendSeconds = 0.f;
    int32  WaitTicks = 0;

    FName   LevelName = NAME_None;
    FVector HoldStartLoc = FVector::ZeroVector;
    FVector HoldDir = FVector::ForwardVector;

    UPROPERTY(Transient)
    TObjectPtr<ACameraActor> IntroCamera = nullptr;

    TWeakObjectPtr<APlayerController> CachedPC;
    TWeakObjectPtr<AkdMyPlayer>       CachedPlayer;
};