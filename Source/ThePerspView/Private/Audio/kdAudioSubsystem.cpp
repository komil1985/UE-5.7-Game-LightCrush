// Copyright ASKD Games


#include "Audio/kdAudioSubsystem.h"
#include "Data/kdAudioBank.h"
#include "Audio/kdAudioSettings.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void UkdAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadBank();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("UkdAudioSubsystem: Initialized. Bank=%s"),
        Bank ? *Bank->GetName() : TEXT("NULL — set Project Settings → Game → Heliograph Audio"));
#endif
}

void UkdAudioSubsystem::Deinitialize()
{
    // Stop hard on shutdown — no fade needed, the game is closing.
    if (IsValid(MusicDeckA)) { MusicDeckA->Stop(); }
    if (IsValid(MusicDeckB)) { MusicDeckB->Stop(); }
    MusicDeckA = nullptr;
    MusicDeckB = nullptr;
    CurrentTrack = nullptr;

    Super::Deinitialize();
}

UkdAudioSubsystem* UkdAudioSubsystem::Get(const UObject* WorldContext)
{
    if (!WorldContext) return nullptr;
    const UWorld* World = WorldContext->GetWorld();
    if (!World) return nullptr;
    UGameInstance* GI = World->GetGameInstance();
    return GI ? GI->GetSubsystem<UkdAudioSubsystem>() : nullptr;
}

void UkdAudioSubsystem::LoadBank()
{
    const UkdAudioSettings* Settings = GetDefault<UkdAudioSettings>();
    if (!Settings) return;

    // Synchronous load — it's one small DataAsset and we want it ready before
    // the menu's first frame so RequestMenuMusic() never misses.
    Bank = Settings->AudioBank.LoadSynchronous();

    if (!Bank)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UkdAudioSubsystem: No Audio Bank assigned (Project Settings → Game → Heliograph Audio)."));
    }
}

// ─── Deck management ──────────────────────────────────────────────────────────

UAudioComponent* UkdAudioSubsystem::SpawnPersistentDeck()
{
    // bPersistAcrossLevelTransition keeps the component alive through OpenLevel.
    // bAutoDestroy=false because WE own its lifetime (ping-pong reuse).
    // Starting silent (volume 0) + not auto-playing; RequestMusic fades it in.
    UAudioComponent* Deck = UGameplayStatics::SpawnSound2D(
        this,
        /*Sound*/ nullptr,
        /*VolumeMultiplier*/ 0.f,
        /*PitchMultiplier*/ 1.f,
        /*StartTime*/ 0.f,
        /*ConcurrencySettings*/ nullptr,
        /*bPersistAcrossLevelTransition*/ true,
        /*bAutoDestroy*/ false);

    if (Deck && Bank && Bank->MusicClass)
    {
        // Route through SC_Music so the Music slider (SoundMix) governs volume.
        Deck->SoundClassOverride = Bank->MusicClass;
    }
    return Deck;
}

void UkdAudioSubsystem::EnsureDecks()
{
    // Decks can go invalid if the audio device was unavailable at spawn time
    // (e.g. cooking/headless). Re-create lazily and defensively.
    if (!IsValid(MusicDeckA)) { MusicDeckA = SpawnPersistentDeck(); }
    if (!IsValid(MusicDeckB)) { MusicDeckB = SpawnPersistentDeck(); }
}

UAudioComponent* UkdAudioSubsystem::ActiveMusic() const
{
    return (ActiveDeck == 0) ? MusicDeckA : MusicDeckB;
}

UAudioComponent* UkdAudioSubsystem::InactiveMusic() const
{
    return (ActiveDeck == 0) ? MusicDeckB : MusicDeckA;
}

// ─── Music ────────────────────────────────────────────────────────────────────

void UkdAudioSubsystem::RequestMusic(USoundBase* Track, float FadeSeconds)
{
    if (!Track) return;
    EnsureDecks();
    if (!IsValid(MusicDeckA) || !IsValid(MusicDeckB)) return;   // no audio device

    // Same track already playing → let it ride. This is what makes music
    // continue seamlessly when two consecutive levels share a track.
    if (CurrentTrack == Track && ActiveMusic() && ActiveMusic()->IsPlaying())
    {
        return;
    }

    const float Fade = (FadeSeconds >= 0.f)
        ? FadeSeconds
        : (Bank ? Bank->MusicCrossfadeSeconds : 1.5f);

    const float TargetVol = (bCrushFilterActive && Bank && Bank->bDuckMusicInCrush)
        ? Bank->CrushMusicVolume
        : 1.f;

    UAudioComponent* Outgoing = ActiveMusic();
    UAudioComponent* Incoming = InactiveMusic();

    // Bring the incoming deck up on the new track.
    Incoming->SetSound(Track);
    Incoming->SetLowPassFilterEnabled(false);
    Incoming->FadeIn(Fade, TargetVol, 0.f);

    // Retire the outgoing deck.
    if (Outgoing && Outgoing->IsPlaying())
    {
        Outgoing->FadeOut(Fade, 0.f);
    }

    // If we're mid-crush, the incoming deck should inherit the muffle.
    if (bCrushFilterActive)
    {
        ApplyCrushFilterToDeck(Incoming, true, /*bInstant*/ true);
    }

    ActiveDeck = 1 - ActiveDeck;
    CurrentTrack = Track;

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("UkdAudioSubsystem: music → %s (fade %.2fs)"), *Track->GetName(), Fade);
#endif
}

void UkdAudioSubsystem::RequestMusicForLevel(FName LevelName)
{
    if (!Bank) return;

    USoundBase* Track = nullptr;
    if (const TObjectPtr<USoundBase>* Found = Bank->PerLevelMusic.Find(LevelName))
    {
        Track = *Found;
    }
    if (!Track)
    {
        Track = Bank->DefaultGameplayMusic;
    }
    RequestMusic(Track);
}

void UkdAudioSubsystem::RequestMenuMusic()
{
    if (Bank) { RequestMusic(Bank->MenuMusic); }
}

void UkdAudioSubsystem::StopMusic(float FadeSeconds)
{
    if (IsValid(MusicDeckA) && MusicDeckA->IsPlaying()) { MusicDeckA->FadeOut(FadeSeconds, 0.f); }
    if (IsValid(MusicDeckB) && MusicDeckB->IsPlaying()) { MusicDeckB->FadeOut(FadeSeconds, 0.f); }
    CurrentTrack = nullptr;
}

void UkdAudioSubsystem::SetCrushMusicFilter(bool bEnabled)
{
    if (!Bank || !Bank->bDuckMusicInCrush) return;
    if (bCrushFilterActive == bEnabled) return;
    bCrushFilterActive = bEnabled;

    ApplyCrushFilterToDeck(ActiveMusic(), bEnabled, /*bInstant*/ false);
}

void UkdAudioSubsystem::ApplyCrushFilterToDeck(UAudioComponent* Deck, bool bEnabled, bool bInstant)
{
    if (!IsValid(Deck) || !Bank) return;

    const float Ramp = bInstant ? 0.f : Bank->CrushDuckRamp;

    if (bEnabled)
    {
        Deck->SetLowPassFilterFrequency(Bank->CrushMusicLowPassHz);
        Deck->SetLowPassFilterEnabled(true);
        // AdjustVolume ramps the multiplier without restarting the track.
        Deck->AdjustVolume(Ramp, Bank->CrushMusicVolume);
    }
    else
    {
        Deck->SetLowPassFilterEnabled(false);
        Deck->AdjustVolume(Ramp, 1.f);
    }
}

// ─── SFX ──────────────────────────────────────────────────────────────────────

void UkdAudioSubsystem::PlaySFX2D(USoundBase* Sound, float VolumeMultiplier, float PitchMultiplier)
{
    if (!Sound) return;
    // Fire-and-forget; engine auto-destroys the transient component.
    UGameplayStatics::SpawnSound2D(this, Sound, VolumeMultiplier, PitchMultiplier);
}

// ─── Semantic event hooks ─────────────────────────────────────────────────────

void UkdAudioSubsystem::OnCrushEnterStarted()
{
    if (!Bank) return;
    PlaySFX2D(Bank->CrushEnterStart);
    SetCrushMusicFilter(true);
}

void UkdAudioSubsystem::OnCrushExitStarted()
{
    if (!Bank) return;
    PlaySFX2D(Bank->CrushExitStart);
    SetCrushMusicFilter(false);
}

void UkdAudioSubsystem::OnCrushLanded(bool /*bNowCrush*/)
{
    if (Bank) { PlaySFX2D(Bank->CrushLand); }
}

void UkdAudioSubsystem::OnCrushDenied()
{
    if (Bank) { PlaySFX2D(Bank->CrushDenied); }
}

void UkdAudioSubsystem::OnShadowChanged(bool bInShadow)
{
    if (!Bank) return;
    PlaySFX2D(bInShadow ? Bank->ShadowEnter : Bank->ShadowExit);
}

void UkdAudioSubsystem::OnUIClick() { if (Bank) PlaySFX2D(Bank->UIClick); }
void UkdAudioSubsystem::OnUIBack() { if (Bank) PlaySFX2D(Bank->UIBack); }
void UkdAudioSubsystem::OnUIHover() { if (Bank) PlaySFX2D(Bank->UIHover, 0.6f); }

void UkdAudioSubsystem::OnLevelComplete()
{
    if (!Bank) return;
    PlaySFX2D(Bank->LevelComplete);
    if (Bank->LevelCompleteSting)
    {
        // Layer the musical sting on top of the (now ducked) loop.
        PlaySFX2D(Bank->LevelCompleteSting);
    }
}

void UkdAudioSubsystem::OnPlayerDeath() { if (Bank) PlaySFX2D(Bank->Death); }
void UkdAudioSubsystem::OnGameOver() { if (Bank) PlaySFX2D(Bank->GameOver); }
