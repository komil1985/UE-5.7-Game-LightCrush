// Copyright ASKD Games


#include "GameInstance/kdGameInstance.h"
#include "SaveGame/kdSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"


const FString UkdGameInstance::SaveSlotName = TEXT("KD_Save_00");


UkdGameInstance::UkdGameInstance()
{
    // Default level order — override in your Blueprint subclass.
    LevelOrder.Append({
        FName("L_MainMenu"),
        FName("L_Level_01"),
        FName("L_Level_02"),
        FName("L_Level_03")
        });
}

void UkdGameInstance::Init()
{
    Super::Init();

    // Load progress first so settings are populated before ApplySettings()
    TryLoadProgress();
    ApplySettings();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("UkdGameInstance: Initialized. LevelOrder has %d entries."), LevelOrder.Num());
#endif
}

void UkdGameInstance::Shutdown()
{
    SaveProgress();
    Super::Shutdown();
}

// ─── Static helper ────────────────────────────────────────────────────────────

UkdGameInstance* UkdGameInstance::Get(const UObject* WorldContext)
{
    if (!WorldContext) return nullptr;
    UWorld* World = WorldContext->GetWorld();
    if (!World) return nullptr;
    return Cast<UkdGameInstance>(World->GetGameInstance());
}

// ─── Level Navigation ─────────────────────────────────────────────────────────

void UkdGameInstance::LoadLevel(FName LevelName)
{
    // Reset session data every time we load a new level
    LevelStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    SessionScore = 0;
    InternalLoadLevel(LevelName);
}

void UkdGameInstance::LoadNextLevel()
{
    const int32 NextIndex = CurrentLevelIndex + 1;
    if (LevelOrder.IsValidIndex(NextIndex))
    {
        CurrentLevelIndex = NextIndex;
        LoadLevel(LevelOrder[NextIndex]);
    }
    else
    {
        // All levels complete — return to main menu
        LoadMainMenu();
    }
}

void UkdGameInstance::ReloadCurrentLevel()
{
    if (LevelOrder.IsValidIndex(CurrentLevelIndex))
    {
        LoadLevel(LevelOrder[CurrentLevelIndex]);
    }
}

void UkdGameInstance::LoadMainMenu()
{
    CurrentLevelIndex = 0;
    LevelStartTime = 0.f;
    SessionScore = 0;
    InternalLoadLevel(MainMenuLevelName);
}

bool UkdGameInstance::HasNextLevel() const
{
    return LevelOrder.IsValidIndex(CurrentLevelIndex + 1);
}

// ─── Internal load ────────────────────────────────────────────────────────────

void UkdGameInstance::InternalLoadLevel(FName LevelName)
{
    UGameplayStatics::OpenLevel(GetWorld(), LevelName);
}

// ─── Settings ─────────────────────────────────────────────────────────────────

void UkdGameInstance::CommitSettings(const FkdGameSettings& NewSettings)
{
    GameSettings = NewSettings;
    ApplySettings();
    SaveProgress();   // persist immediately so crash doesn't lose settings
}

void UkdGameInstance::ApplySettings()
{
    ApplyAudioSettings();
    ApplyGraphicsSettings();
    OnSettingsApplied.Broadcast();
}

void UkdGameInstance::ApplyAudioSettings()
{
    // Sound Mix override — requires MasterSoundMix to be assigned in BP defaults.
    // If you haven't set up a SoundMix yet, these calls are harmless no-ops.
    UWorld* World = GetWorld();
    if (!World || !MasterSoundMix) return;

    auto ApplyClass = [&](USoundClass* SClass, float Volume)
        {
            if (SClass)
            {
                UGameplayStatics::SetSoundMixClassOverride(
                    World, MasterSoundMix, SClass,
                    Volume,     // volume multiplier
                    1.0f,       // pitch multiplier
                    0.1f,       // fade-in time
                    true);      // apply to children
            }
        };

    UGameplayStatics::PushSoundMixModifier(World, MasterSoundMix);
    ApplyClass(MasterSoundClass, GameSettings.MasterVolume);
    ApplyClass(MusicSoundClass, GameSettings.MusicVolume);
    ApplyClass(SFXSoundClass, GameSettings.SFXVolume);
}

void UkdGameInstance::ApplyGraphicsSettings()
{
    UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (!UserSettings) return;

    UserSettings->SetOverallScalabilityLevel(GameSettings.QualityPreset);
    UserSettings->SetVSyncEnabled(GameSettings.bVSync);
    UserSettings->SetFullscreenMode(
        GameSettings.bFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);

    // Only call ApplySettings once — it recompiles shaders if quality changed.
    // bCheckForCommandLineOverrides=false keeps our values authoritative.
    UserSettings->ApplySettings(false);
}

// ─── Save / Load ──────────────────────────────────────────────────────────────

void UkdGameInstance::SaveProgress()
{
    if (!SaveGameObject)
    {
        SaveGameObject = Cast<UkdSaveGame>(UGameplayStatics::CreateSaveGameObject(UkdSaveGame::StaticClass()));
    }
    if (!SaveGameObject) return;

    SaveGameObject->Settings = GameSettings;
    UGameplayStatics::SaveGameToSlot(SaveGameObject, SaveSlotName, SaveUserIndex);
}

bool UkdGameInstance::TryLoadProgress()
{
    if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
    {
        // First launch — create a default save object but don't write to disk yet
        SaveGameObject = Cast<UkdSaveGame>(
            UGameplayStatics::CreateSaveGameObject(UkdSaveGame::StaticClass()));
        return false;
    }

    SaveGameObject = Cast<UkdSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));

    if (!SaveGameObject)
    {
        SaveGameObject = Cast<UkdSaveGame>(
            UGameplayStatics::CreateSaveGameObject(UkdSaveGame::StaticClass()));
        return false;
    }

    GameSettings = SaveGameObject->Settings;

#if !UE_BUILD_SHIPPING
    //UE_LOG(LogTemp, Log, TEXT("UkdGameInstance: Save loaded (v%d)."), SaveGameObject->SaveVersion);
#endif

    return true;
}

int32 UkdGameInstance::GetHighScore(int32 LevelIndex) const
{
    return SaveGameObject ? SaveGameObject->GetHighScore(LevelIndex) : 0;
}

float UkdGameInstance::GetBestTime(int32 LevelIndex) const
{
    return SaveGameObject ? SaveGameObject->GetBestTime(LevelIndex) : 0.f;
}

bool UkdGameInstance::IsLevelUnlocked(int32 LevelIndex) const
{
    return SaveGameObject ? SaveGameObject->IsLevelUnlocked(LevelIndex) : (LevelIndex == 0);
}

void UkdGameInstance::RecordLevelComplete(int32 LevelIndex, float CompletionTimeSeconds, int32 Score)
{
    if (!SaveGameObject) return;

    const bool bNewScore = SaveGameObject->TrySetHighScore(LevelIndex, Score);
    const bool bNewTime = SaveGameObject->TrySetBestTime(LevelIndex, CompletionTimeSeconds);

    // Unlock the next level automatically
    SaveGameObject->UnlockLevel(LevelIndex + 1);

    SaveProgress();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("RecordLevelComplete: Level=%d  Time=%.1fs  Score=%d  NewBestScore=%d  NewBestTime=%d"),
        LevelIndex, CompletionTimeSeconds, Score, bNewScore ? 1 : 0, bNewTime ? 1 : 0);
#endif
}
