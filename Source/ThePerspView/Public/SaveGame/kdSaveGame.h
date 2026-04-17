// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "kdSaveGame.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Persisted player settings — lives in the save file so options survive
// between sessions without requiring the engine's .ini system.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct THEPERSPVIEW_API FkdGameSettings
{
    GENERATED_BODY()

    // Audio
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Audio")
    float MasterVolume = 1.0f;

    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Audio")
    float MusicVolume = 0.8f;

    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Audio")
    float SFXVolume = 1.0f;

    // Graphics
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Graphics")
    int32 QualityPreset = 2;        // 0=Low  1=Medium  2=High  3=Epic

    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Graphics")
    bool bFullscreen = true;

    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Graphics")
    bool bVSync = false;

    // Controls
    UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Controls")
    float MouseSensitivity = 1.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// Save Game Object — written to disk on every meaningful change.
// Slot name: "KD_Save_00"   User index: 0
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class THEPERSPVIEW_API UkdSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
    UkdSaveGame();

    // Bump this when the serialized layout changes so we can migrate old saves
    static constexpr int32 CurrentSaveVersion = 2;

    UPROPERTY(SaveGame)
    int32 SaveVersion = CurrentSaveVersion;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FkdGameSettings Settings;

    // Per-level records (array index == level index in LevelOrder)
    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<int32> LevelHighScores;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<float> LevelBestTimes;   // seconds; 0 = never completed

    // Levels the player has reached at least once
    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<int32> UnlockedLevels;

    // ── Helpers ──────────────────────────────────────────────────────────────
    int32  GetHighScore(int32 LevelIdx) const;
    bool   TrySetHighScore(int32 LevelIdx, int32 Score);        // true if new best

    float  GetBestTime(int32 LevelIdx) const;
    bool   TrySetBestTime(int32 LevelIdx, float Seconds);       // true if new best

    bool   IsLevelUnlocked(int32 LevelIdx) const;
    void   UnlockLevel(int32 LevelIdx);

private:
    void EnsureCapacity(int32 LevelIdx);
	
};
