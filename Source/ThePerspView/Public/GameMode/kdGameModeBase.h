// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "kdGameModeBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLevelComplete, float, CompletionTime, int32, FinalScore, bool, bNewBestTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerRespawned, int32, RemainingLives);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDied, int32, RemainingLives);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameOver, int32, FinalScore);


class AkdMyPlayer;
class AkdCheckpoint;
class UkdHUD;
class UkdDeathComponent;
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

    UFUNCTION(BlueprintPure, Category = "State")
    bool IsLevelComplete() const { return bLevelComplete; }

    UFUNCTION(BlueprintPure, Category = "State")
    bool IsGameOver() const { return bGameOver; }

    // ── Respawn ───────────────────────────────────────────────────────────────

    /** AkdCheckpoint calls this when the player touches it. */
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void RegisterCheckpoint(AkdCheckpoint* NewCheckpoint);

    /** Called by the player (or attribute set) when they've lost a life. */
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void HandlePlayerDeath();

    UPROPERTY(BlueprintAssignable, Category = "Death")
    FOnPlayerDied OnPlayerDied;

    UPROPERTY(BlueprintAssignable, Category = "Death")
    FOnGameOver OnGameOver;

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
    bool  bGameOver = false;

    FTimerHandle RespawnTimerHandle;

    void PerformRespawn();
    AkdMyPlayer* GetCachedPlayer() const;

    // Bound in BeginPlay — listens for the DeathComponent to finish its sequence
    UFUNCTION()
    void OnDeathSequenceFinished();

    void TriggerGameOver();
    UkdDeathComponent* FindDeathComponent() const;
	
};

// ─────────────────────────────────────────────────────────────────────────────
// AkdGameModeBase — drives every gameplay-level session.
//
// DEATH FLOW:
//
//   PostGameplayEffectExecute (stamina = 0)
//           │
//   HandlePlayerDeath()            ← entry point for all death paths
//           │
//   DeathComponent->TriggerDeath() ← visual sequence (fade, slow-mo, mesh hide)
//           │
//   [DeathFadeDuration seconds]
//           │
//   OnDeathSequenceComplete        ← DeathComponent fires this delegate
//           │
//   OnDeathSequenceFinished()      ← GameMode slot: check lives
//       ├── Lives > 0 ──▶ HUD::ShowDeathOverlay  ──▶ wait RespawnDelay
//       │                       │
//       │               PerformRespawn()
//       │               DeathComponent->TriggerRespawn()
//       │               HUD::HideDeathOverlay
//       │
//       └── Lives == 0 ─▶ HUD::ShowGameOver  (game stops here)
//
// SETUP:
//   1. AkdMyPlayer must have a UkdDeathComponent attached (created in constructor).
//   2. This GameMode automatically binds to DeathComponent->OnDeathSequenceComplete
//      in BeginPlay via FindDeathComponent().
// ─────────────────────────────────────────────────────────────────────────────