// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "kdAudioSubsystem.generated.h"


class UkdAudioBank;
class USoundBase;
class UAudioComponent;

// ─────────────────────────────────────────────────────────────────────────────
// UkdAudioSubsystem — the single owner of music playback and SFX dispatch.
//
// WHY A GAME-INSTANCE SUBSYSTEM:
//   You travel between levels with UGameplayStatics::OpenLevel (a hard load),
//   which destroys every actor — and therefore every world-spawned
//   UAudioComponent. A GameInstanceSubsystem survives that travel, so it can
//   keep a music track playing seamlessly across L_Level_01 → L_Level_02, and
//   crossfade only when the *track itself* changes (e.g. gameplay → menu).
//
// DESIGN (mirrors your Heliograph theme system):
//   • UkdAudioBank        = the sonic theme   (≈ UkdColorTheme / DA_Heliograph)
//   • UkdAudioSettings    = project pointer    (≈ UkdThemeSettings)
//   • UkdAudioSubsystem   = the runtime owner   (≈ UkdThemeSubsystem)
//
// OWNERSHIP CONTRACT (keep it like your WorldColorDriver rule):
//   • This subsystem is the ONLY thing that starts/stops/ducks music.
//   • Category volume (Master / Music / SFX sliders) stays owned by
//     UkdGameInstance via its SoundMix — this subsystem never touches it.
//     The two layers compose; they never fight.
//
// TWO-DECK CROSSFADER:
//   Two persistent UAudioComponents ping-pong. The incoming deck fades in while
//   the outgoing deck fades out — a true crossfade, not a gap or a hard cut.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class THEPERSPVIEW_API UkdAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Convenience accessor: UkdAudioSubsystem::Get(this)->RequestMenuMusic(); */
    static UkdAudioSubsystem* Get(const UObject* WorldContext);

    // ── Music ─────────────────────────────────────────────────────────────────

    /** Crossfade to Track. No-op if Track is already the active music (so it
     *  continues seamlessly across same-track level loads). Fade < 0 = bank default. */
    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Music")
    void RequestMusic(USoundBase* Track, float FadeSeconds = -1.f);

    /** Resolve and play the track for a level FName (PerLevelMusic ⟶ default). */
    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Music")
    void RequestMusicForLevel(FName LevelName);

    /** Play the menu track. Called from AkdMainMenuGameMode::BeginPlay. */
    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Music")
    void RequestMenuMusic();

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Music")
    void StopMusic(float FadeSeconds = 1.0f);

    /** Muffle + lower the music for Crush Mode (true) or restore it (false). */
    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Music")
    void SetCrushMusicFilter(bool bEnabled);

    // ── SFX ───────────────────────────────────────────────────────────────────

    /** Null-safe 2D one-shot. Use this for everything non-spatial (UI, stingers,
     *  the crush whooshes — the camera is far away in 2D so 2D SFX read cleaner). */
    UFUNCTION(BlueprintCallable, Category = "kd|Audio|SFX")
    void PlaySFX2D(USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

    // ── Semantic event hooks (call these from gameplay; they read the bank) ────
    // Keeping the vocabulary here means callers never reach into the bank.

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnCrushEnterStarted();                 // GFC::OnCrushTransitionStarted(true)

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnCrushExitStarted();                  // GFC::OnCrushTransitionStarted(false)

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnCrushLanded(bool bNowCrush);         // GFC::OnCrushTransitionFinished

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnCrushDenied();                       // kd_CrushToggle off-axis abort

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnShadowChanged(bool bInShadow);       // GFC::OnInShadowTagChanged

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnUIClick();

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnUIBack();

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnUIHover();

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnLevelComplete();                     // GameMode::TriggerLevelComplete

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnPlayerDeath();

    UFUNCTION(BlueprintCallable, Category = "kd|Audio|Events")
    void OnGameOver();

    /** The resolved bank (may be null if Project Settings isn't configured). */
    UFUNCTION(BlueprintPure, Category = "kd|Audio")
    const UkdAudioBank* GetBank() const { return Bank; }

private:
    // Resolved once in Initialize from UkdAudioSettings. Loaded synchronously —
    // a single small DataAsset; fine at startup, avoids first-sound hitch.
    UPROPERTY()
    TObjectPtr<const UkdAudioBank> Bank = nullptr;

    // Two persistent music decks for crossfading. UPROPERTY = GC-safe.
    UPROPERTY()
    TObjectPtr<UAudioComponent> MusicDeckA = nullptr;

    UPROPERTY()
    TObjectPtr<UAudioComponent> MusicDeckB = nullptr;

    // Track currently faded *in* (drives the same-track no-op).
    UPROPERTY()
    TObjectPtr<USoundBase> CurrentTrack = nullptr;

    int32  ActiveDeck = 0;            // 0 = A, 1 = B
    bool   bCrushFilterActive = false;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void LoadBank();
    void EnsureDecks();                          // (re)create decks if invalid
    UAudioComponent* SpawnPersistentDeck();
    UAudioComponent* ActiveMusic() const;
    UAudioComponent* InactiveMusic() const;
    void ApplyCrushFilterToDeck(UAudioComponent* Deck, bool bEnabled, bool bInstant);
};
