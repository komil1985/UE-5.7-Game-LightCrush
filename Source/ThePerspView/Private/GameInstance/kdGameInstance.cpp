// Copyright ASKD Games


#include "GameInstance/kdGameInstance.h"
#include "SaveGame/kdSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Audio/kdAudioSubsystem.h"
#include "Misc/PackageName.h"


const FString UkdGameInstance::SaveSlotName = TEXT("KD_Save_00");


UkdGameInstance::UkdGameInstance()
{
    // Default level order — override in your Blueprint subclass.
    LevelOrder.Append({
        FName("L_MainMenu"),
        FName("L_Level_01"),
        FName("L_Level_02"),
        FName("L_Level_03"),
        FName("L_Level_04"),
        FName("L_Level_05")
        });
}

void UkdGameInstance::Init()
{
    Super::Init();
    TryLoadProgress();
    ApplySettings();

    // Central music router — fires after every OpenLevel in all build types.
    // Individual GameModes never need to know which track belongs to which level;
    // the bank's PerLevelMusic TMap is the single source of truth.
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UkdGameInstance::OnPostLoadMap);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("UkdGameInstance: Initialized. LevelOrder has %d entries."), LevelOrder.Num());
#endif
}

void UkdGameInstance::Shutdown()
{
    // Must unbind before Super — otherwise the delegate fires on a half-dead object.
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
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

bool UkdGameInstance::HasSeenTutorial(FName StepId) const
{
    return SaveGameObject ? SaveGameObject->HasSeenTutorial(StepId) : false;
}

void UkdGameInstance::MarkTutorialSeen(FName StepId)
{
    if (!SaveGameObject) return;

    // Only hit the disk when the set actually changed.
    if (SaveGameObject->MarkTutorialSeen(StepId))
    {
        SaveProgress();
    }
}

// ─── Level Navigation ─────────────────────────────────────────────────────────

void UkdGameInstance::LoadLevel(FName LevelName)
{
    const int32 Found = LevelOrder.IndexOfByKey(LevelName);
    if (Found != INDEX_NONE) CurrentLevelIndex = Found;

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

void UkdGameInstance::LoadCurrentLevel()
{
    if (!LevelOrder.IsValidIndex(CurrentLevelIndex))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("LoadCurrentLevel: index %d out of range (%d levels)."),
            CurrentLevelIndex, LevelOrder.Num());
        return;
    }

    UGameplayStatics::OpenLevel(this, LevelOrder[CurrentLevelIndex]);
}

int32 UkdGameInstance::GetTotalLevelCount() const
{
    return LevelOrder.Num();
}

void UkdGameInstance::SetLastPlayedLevelIndex(int32 Index)
{
    LastPlayedLevelIndex = Index;

    // Persist immediately so a crash mid-level still remembers where the
    // player was.
    SaveProgress();
}

// ─── Internal load ────────────────────────────────────────────────────────────

void UkdGameInstance::InternalLoadLevel(FName LevelName)
{
    UGameplayStatics::OpenLevel(GetWorld(), LevelName);
}

void UkdGameInstance::OnPostLoadMap(UWorld* LoadedWorld)
{
    // Re-entry guard: in UE5.7, CreateSound2D with bPersistAcrossLevelTransition
    // registers a persistent component during the loading sequence, which
    // re-broadcasts PostLoadMapWithWorld and causes infinite recursion.
    if (bMusicRouteInProgress || !LoadedWorld) return;
    TGuardValue<bool> ReentryGuard(bMusicRouteInProgress, true);

    UkdAudioSubsystem* Audio = GetSubsystem<UkdAudioSubsystem>();
    if (!Audio) return;

    // StreamingLevelsPrefix == "UEDPIE_0_" in PIE, "" in standalone/shipping.
    // Stripping it gives the bare map name that matches LevelOrder and PerLevelMusic keys.
    FString MapName = LoadedWorld->GetMapName();
    MapName.RemoveFromStart(LoadedWorld->StreamingLevelsPrefix);
    const FName LevelFName(*MapName);

    UE_LOG(LogTemp, Log, TEXT("[GameInstance] OnPostLoadMap → '%s'"), *MapName);

    // Defer by one tick — places the audio call completely outside the
    // PostLoadMapWithWorld broadcast stack so CreateSound2D cannot re-trigger it.
    TWeakObjectPtr<UkdAudioSubsystem> AudioWeak(Audio);
    const bool   bIsMenu = (LevelFName == MainMenuLevelName);
    const FName  CapturedName = LevelFName;

    LoadedWorld->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateLambda([AudioWeak, CapturedName, bIsMenu]()
            {
                UkdAudioSubsystem* A = AudioWeak.Get();
                if (!IsValid(A)) return;

                if (bIsMenu) A->RequestMenuMusic();
                else         A->RequestMusicForLevel(CapturedName);
            }));
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
    UserSettings->SetFullscreenMode(GameSettings.bFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);
    
    if (GameSettings.ScreenResolution.X > 0 && GameSettings.ScreenResolution.Y > 0)
    {
        UserSettings->SetScreenResolution(GameSettings.ScreenResolution);
    }

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
    SaveGameObject->LastPlayedLevelIndex = LastPlayedLevelIndex;
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
    LastPlayedLevelIndex = SaveGameObject->LastPlayedLevelIndex;

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("UkdGameInstance: Save loaded. Version (v%d)."), SaveGameObject->SaveVersion);
    UE_LOG(LogTemp, Log, TEXT("UkdGameInstance: Save loaded. LastPlayedLevel=%d"),LastPlayedLevelIndex);
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

    // Track where the player is so Continue always resumes correctly.
    // We store the NEXT level index — that's where they should continue from.
    const int32 NextIndex = LevelIndex + 1;
    if (LevelOrder.IsValidIndex(NextIndex))
    {
        LastPlayedLevelIndex = NextIndex;
    }

    SaveProgress();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("RecordLevelComplete: Level=%d  Time=%.1fs  Score=%d  NewBestScore=%d  NewBestTime=%d"),
        LevelIndex, CompletionTimeSeconds, Score, bNewScore ? 1 : 0, bNewTime ? 1 : 0);
#endif
}
