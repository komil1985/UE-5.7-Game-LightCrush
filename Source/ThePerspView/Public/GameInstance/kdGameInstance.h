// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SaveGame/kdSaveGame.h"
#include "kdGameInstance.generated.h"


//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsApplied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingApplied);


class USoundMix;
class USoundClass;
struct FkdGameSettings;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
    UkdGameInstance();

    virtual void Init() override;
    virtual void Shutdown() override;

    // ── Level Navigation ─────────────────────────────────────────────────────

    /** Load any level by name. Shows the loading screen automatically. */
    UFUNCTION(BlueprintCallable, Category = "Level")
    void LoadLevel(FName LevelName);

    /** Advance to the next entry in LevelOrder. Goes to main menu if at the end. */
    UFUNCTION(BlueprintCallable, Category = "Level")
    void LoadNextLevel();

    /** Reload the current level (respawns everything, resets session data). */
    UFUNCTION(BlueprintCallable, Category = "Level")
    void ReloadCurrentLevel();

    /** Unconditionally return to the main menu level. */
    UFUNCTION(BlueprintCallable, Category = "Level")
    void LoadMainMenu();

    UFUNCTION(BlueprintCallable, Category = "Level")
    void LoadTutorial();

    UFUNCTION(BlueprintPure, Category = "Level")
    int32 GetCurrentLevelIndex() const { return CurrentLevelIndex; }

    UFUNCTION(BlueprintCallable, Category = "Level")
    void SetCurrentLevelIndex(int32 Index) { CurrentLevelIndex = Index; }

    UFUNCTION(BlueprintPure, Category = "Level")
    bool HasNextLevel() const;

    /** Opens Levels[CurrentLevelIndex] directly — no increment. */
    UFUNCTION(BlueprintCallable, Category = "Level")
    void LoadCurrentLevel();

    /** Returns the total number of entries in the Levels array. */
    UFUNCTION(BlueprintPure, Category = "Level")
    int32 GetTotalLevelCount() const;

    UFUNCTION(BlueprintPure, Category = "Level")
    int32 GetLastPlayedLevelIndex() const { return LastPlayedLevelIndex; }

    /** Called whenever the player enters a playable level. */
    void SetLastPlayedLevelIndex(int32 Index);

    // ── Settings ─────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Settings")
    FkdGameSettings GetGameSettings() const { return GameSettings; }

    /** Apply, persist to disk, and broadcast OnSettingsApplied. */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void CommitSettings(const FkdGameSettings& NewSettings);

    /** Re-apply current settings without saving (e.g. after loading). */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplySettings();

    UPROPERTY(BlueprintAssignable, Category = "Settings")
    FOnSettingApplied OnSettingsApplied;

    // ── Sound Assets (assign in BP subclass or Details panel) ─────────────────
    UPROPERTY(EditDefaultsOnly, Category = "Audio")
    TObjectPtr<USoundMix> MasterSoundMix;

    UPROPERTY(EditDefaultsOnly, Category = "Audio")
    TObjectPtr<USoundClass> MasterSoundClass;

    UPROPERTY(EditDefaultsOnly, Category = "Audio")
    TObjectPtr<USoundClass> MusicSoundClass;

    UPROPERTY(EditDefaultsOnly, Category = "Audio")
    TObjectPtr<USoundClass> SFXSoundClass;

    // ── Save / Load ───────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Save")
    void SaveProgress();

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool TryLoadProgress();

    UFUNCTION(BlueprintPure, Category = "Save")
    int32 GetHighScore(int32 LevelIndex) const;

    UFUNCTION(BlueprintPure, Category = "Save")
    float GetBestTime(int32 LevelIndex) const;

    UFUNCTION(BlueprintPure, Category = "Save")
    bool IsLevelUnlocked(int32 LevelIndex) const;

    // Called by GameMode when the player completes a level
    UFUNCTION(BlueprintCallable, Category = "Save")
    void RecordLevelComplete(int32 LevelIndex, float CompletionTimeSeconds, int32 Score);

    // ── Session Data (transient, reset each level load) ───────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Session")
    float LevelStartTime = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Session")
    int32 SessionScore = 0;

    // ── Level Config (populate in Blueprint subclass) ────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Levels")
    TArray<FName> LevelOrder;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Levels")
    FName MainMenuLevelName = FName("L_MainMenu");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Levels")
    FName TutorialLevelName = FName("L_Tutorial");

    // ── Statics ───────────────────────────────────────────────────────────────
    static UkdGameInstance* Get(const UObject* WorldContext);

    /** Map name at a LevelOrder index, or NAME_None if out of range. */
    UFUNCTION(BlueprintCallable, Category = "Level")
    FName GetLevelNameAtIndex(int32 Index) const
    {
        return LevelOrder.IsValidIndex(Index) ? LevelOrder[Index] : NAME_None;
    }

    /** The main-menu map name (LevelOrder index 0 in the default setup). */
    UFUNCTION(BlueprintCallable, Category = "Level")
    FName GetMainMenuLevelName() const { return MainMenuLevelName; }

    /* Tutorial Tracking */
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    bool HasSeenTutorial(FName StepId) const;

    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void MarkTutorialSeen(FName StepId);

private:
    UPROPERTY()
    FkdGameSettings GameSettings;

    UPROPERTY()
    TObjectPtr<UkdSaveGame> SaveGameObject;

    int32 CurrentLevelIndex = 0;
    int32 LastPlayedLevelIndex = 1;

    static const FString SaveSlotName;
    static constexpr int32 SaveUserIndex = 0;

    void ApplyAudioSettings();
    void ApplyGraphicsSettings();
    void InternalLoadLevel(FName LevelName);

    /** Re-entry guard: PostLoadMapWithWorld re-fires in UE5.7 when CreateSound2D
    registers a persistent (bIgnoreForFlushing) component during level load. */
    bool bMusicRouteInProgress = false;

    /** Fired by the engine after every OpenLevel — the single place that
    *  routes music so no individual GameMode needs to know which track to play. */
    void OnPostLoadMap(UWorld* LoadedWorld);
	
};
