// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "kdGameModeBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLevelComplete, float, CompletionTime, int32, FinalScore, bool, bNewBestTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerRespawned, int32, RemainingLives);


class AkdMyPlayer;
class AkdCheckpoint;
class UkdHUD;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
    AkdGameModeBase();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // ── Level Complete ────────────────────────────────────────────────────────

    /** Called by AkdLevelGoal when the player reaches the exit. */
    UFUNCTION(BlueprintCallable, Category = "Level")
    void TriggerLevelComplete();

    UPROPERTY(BlueprintAssignable, Category = "Level")
    FOnLevelComplete OnLevelComplete;

    // ── Respawn ───────────────────────────────────────────────────────────────

    /** AkdCheckpoint calls this when the player touches it. */
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void RegisterCheckpoint(AkdCheckpoint* NewCheckpoint);

    /** Called by the player (or attribute set) when they've lost a life. */
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void HandlePlayerDeath();

    UPROPERTY(BlueprintAssignable, Category = "Respawn")
    FOnPlayerRespawned OnPlayerRespawned;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn")
    int32 StartingLives = 3;

    UFUNCTION(BlueprintPure, Category = "Respawn")
    int32 GetRemainingLives() const { return RemainingLives; }

    // ── Score ─────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Score")
    void AddScore(int32 Points);

    UFUNCTION(BlueprintPure, Category = "Score")
    int32 GetCurrentScore() const { return CurrentScore; }

    UFUNCTION(BlueprintPure, Category = "Score")
    float GetElapsedTime() const { return ElapsedTime; }

    // ── Pause ─────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void PauseGame();

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void UnpauseGame();

    UFUNCTION(BlueprintPure, Category = "Pause")
    bool IsGamePaused() const { return bGamePaused; }

    // ── Config ────────────────────────────────────────────────────────────────

    /** Seconds before the respawn teleport fires (shows death feedback). */
    UPROPERTY(EditDefaultsOnly, Category = "Respawn")
    float RespawnDelay = 1.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Level")
    FName LevelCompleteWidgetTag = FName("LevelComplete");

protected:
    virtual void RestartPlayer(AController* NewPlayer) override;

private:
    UPROPERTY()
    TObjectPtr<AkdCheckpoint> ActiveCheckpoint;

    int32 RemainingLives = 3;
    int32 CurrentScore = 0;
    float ElapsedTime = 0.f;
    bool  bLevelComplete = false;
    bool  bGamePaused = false;

    FTimerHandle RespawnTimerHandle;

    void PerformRespawn();
    AkdMyPlayer* GetCachedPlayer() const;
	
};

// ─────────────────────────────────────────────────────────────────────────────
// AkdGameModeBase — gameplay level mode.
//
// RESPONSIBILITIES:
//   • Session timer & score tracking
//   • Lives system with checkpoint-based respawn
//   • Level Complete flow (broadcasts delegate → HUD shows widget)
//   • Pause / unpause helpers
//
// HOW TO TRIGGER LEVEL COMPLETE:
//   Call GameMode->TriggerLevelComplete() from AkdLevelGoal on player overlap.
//
// HOW RESPAWN WORKS:
//   AkdCheckpoint sets itself as the active checkpoint by calling
//   GameMode->RegisterCheckpoint(this).  On player death the GameMode
//   teleports the player back to that checkpoint transform.
// ─────────────────────────────────────────────────────────────────────────────