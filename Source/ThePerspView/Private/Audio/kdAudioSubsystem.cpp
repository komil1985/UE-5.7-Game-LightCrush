// Copyright ASKD Games


#include "Audio/kdAudioSubsystem.h"
#include "Data/kdAudioBank.h"
#include "Audio/kdAudioSettings.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void UkdAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadBank();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("UkdAudioSubsystem: Initialized. Bank=%s"),
        BankA ? *BankA->GetName() : TEXT("NULL — set Project Settings → Game → Heliograph Audio"));

    UE_LOG(LogTemp, Warning, TEXT("[kdAudio] PlayCrushEnter | BankA=%s | Sound=%s"),
        *GetNameSafe(BankA), *GetNameSafe(BankA ? BankA->CrushEnterStart : nullptr));
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

UkdAudioBank* UkdAudioSubsystem::GetResolvedBank()
{
    if (IsValid(ResolvedBank))
    {
        return ResolvedBank;
    }

    const UkdAudioSettings* Settings = GetDefault<UkdAudioSettings>();
    if (!Settings)
    {
        UE_LOG(LogTemp, Error, TEXT("[kdAudio] UkdAudioSettings missing."));
        return nullptr;
    }

    // CRITICAL: LoadSynchronous, not .Get() — a .Get() on an unloaded soft ref returns null and silently breaks all audio.
    ResolvedBank = Settings->AudioBank.LoadSynchronous();
    if (!IsValid(ResolvedBank))
    {
        UE_LOG(LogTemp, Error, TEXT("[kdAudio] AudioBank failed to resolve from Project Settings."));
    }
    return ResolvedBank;
}

void UkdAudioSubsystem::LoadBank()
{
    const UkdAudioSettings* Settings = GetDefault<UkdAudioSettings>();
    if (!Settings) return;

    // Synchronous load — it's one small DataAsset and we want it ready before
    // the menu's first frame so RequestMenuMusic() never misses.
    BankA = Settings->AudioBank.LoadSynchronous();

    if (!BankA)
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

    if (Deck && BankA && BankA->MusicClass)
    {
        // Route through SC_Music so the Music slider (SoundMix) governs volume.
        Deck->SoundClassOverride = BankA->MusicClass;
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
        : (BankA ? BankA->MusicCrossfadeSeconds : 1.5f);

    const float TargetVol = (bCrushFilterActive && BankA && BankA->bDuckMusicInCrush)
        ? BankA->CrushMusicVolume
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
    if (!BankA) return;

    USoundBase* Track = nullptr;
    if (const TObjectPtr<USoundBase>* Found = BankA->PerLevelMusic.Find(LevelName))
    {
        Track = *Found;
    }
    if (!Track)
    {
        Track = BankA->DefaultGameplayMusic;
    }
    RequestMusic(Track);
}

void UkdAudioSubsystem::RequestMenuMusic()
{
    if (BankA) { RequestMusic(BankA->MenuMusic); }
}

void UkdAudioSubsystem::StopMusic(float FadeSeconds)
{
    if (IsValid(MusicDeckA) && MusicDeckA->IsPlaying()) { MusicDeckA->FadeOut(FadeSeconds, 0.f); }
    if (IsValid(MusicDeckB) && MusicDeckB->IsPlaying()) { MusicDeckB->FadeOut(FadeSeconds, 0.f); }
    CurrentTrack = nullptr;
}

void UkdAudioSubsystem::SetCrushMusicFilter(bool bEnabled)
{
    if (!BankA || !BankA->bDuckMusicInCrush) return;
    if (bCrushFilterActive == bEnabled) return;
    bCrushFilterActive = bEnabled;

    ApplyCrushFilterToDeck(ActiveMusic(), bEnabled, /*bInstant*/ false);
}

void UkdAudioSubsystem::ApplyCrushFilterToDeck(UAudioComponent* Deck, bool bEnabled, bool bInstant)
{
    if (!IsValid(Deck) || !BankA) return;

    const float Ramp = bInstant ? 0.f : BankA->CrushDuckRamp;

    if (bEnabled)
    {
        Deck->SetLowPassFilterFrequency(BankA->CrushMusicLowPassHz);
        Deck->SetLowPassFilterEnabled(true);
        // AdjustVolume ramps the multiplier without restarting the track.
        Deck->AdjustVolume(Ramp, BankA->CrushMusicVolume);
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
    if (!IsValid(Sound))
    {
        UE_LOG(LogTemp, Warning, TEXT("[kdAudio] PlaySFX2D: null Sound — DA field not assigned."));
        return;
    }

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!IsValid(World))
    {
        UE_LOG(LogTemp, Error, TEXT("[kdAudio] PlaySFX2D: World is null — called too early (during subsystem init?). Sound=%s"), *Sound->GetName());
        return;  // Was silently failing before — now you'll see this in log
    }

    // Guard: don't play during loading screens or before first BeginPlay
    if (World->GetNetMode() == NM_MAX || !World->HasBegunPlay())
    {
        UE_LOG(LogTemp, Warning, TEXT("[kdAudio] PlaySFX2D: World not begun play yet. Sound=%s"), *Sound->GetName());
        return;
    }

    UkdAudioBank* Bank = GetResolvedBank();
    USoundClass* SfxClass = Bank ? Bank->SFXClass : nullptr;

    // Fire-and-forget. No retained UAudioComponent -> no GC-eats-my-sound failure mode.
    UGameplayStatics::PlaySound2D(World, Sound, VolumeMultiplier, PitchMultiplier, 0.0f, /*ConcurrencySettings*/ nullptr, /*OwningActor*/ nullptr);

    UE_LOG(LogTemp, Log, TEXT("[kdAudio] PlaySFX2D OK → %s"), *Sound->GetName());
}

void UkdAudioSubsystem::PlayCrushEnter()
{
    UkdAudioBank* Bank = GetResolvedBank();
    PlaySFX2D(Bank ? Bank->CrushEnterStart : nullptr);
}

void UkdAudioSubsystem::PlayCrushExit()
{
    UkdAudioBank* Bank = GetResolvedBank();
    PlaySFX2D(Bank ? Bank->CrushExitStart : nullptr);
}

void UkdAudioSubsystem::PlayCrushLand()
{
    UkdAudioBank* Bank = GetResolvedBank();
    PlaySFX2D(Bank ? Bank->CrushLand : nullptr);
}

void UkdAudioSubsystem::PlayCrushDenied()
{
    UkdAudioBank* Bank = GetResolvedBank();
    PlaySFX2D(Bank ? Bank->CrushDenied : nullptr); // intentionally None in your DA right now
}

// ─── Semantic event hooks ─────────────────────────────────────────────────────

void UkdAudioSubsystem::OnCrushEnterStarted()
{
    if (!BankA) return;
    //PlaySFX2D(BankA->CrushEnterStart);
    SetCrushMusicFilter(true);
}

void UkdAudioSubsystem::OnCrushExitStarted()
{
    if (!BankA) return;
    PlaySFX2D(BankA->CrushExitStart);
    SetCrushMusicFilter(false);
}

void UkdAudioSubsystem::OnCrushLanded(bool /*bNowCrush*/)
{
    if (BankA) { PlaySFX2D(BankA->CrushLand); }
}

void UkdAudioSubsystem::OnCrushDenied()
{
    if (BankA) { PlaySFX2D(BankA->CrushDenied); }
}

void UkdAudioSubsystem::OnShadowChanged(bool bInShadow)
{
    if (!BankA) return;
    PlaySFX2D(bInShadow ? BankA->ShadowEnter : BankA->ShadowExit);
}

void UkdAudioSubsystem::OnUIClick() { if (BankA) PlaySFX2D(BankA->UIClick); }
void UkdAudioSubsystem::OnUIBack() { if (BankA) PlaySFX2D(BankA->UIBack); }
void UkdAudioSubsystem::OnUIHover() { if (BankA) PlaySFX2D(BankA->UIHover, 0.6f); }

void UkdAudioSubsystem::OnLevelComplete()
{
    if (!BankA) return;
    PlaySFX2D(BankA->LevelComplete);
    if (BankA->LevelCompleteSting)
    {
        // Layer the musical sting on top of the (now ducked) loop.
        PlaySFX2D(BankA->LevelCompleteSting);
    }
}

void UkdAudioSubsystem::OnPlayerDeath() { if (BankA) PlaySFX2D(BankA->Death); }
void UkdAudioSubsystem::OnGameOver() { if (BankA) PlaySFX2D(BankA->GameOver); }
