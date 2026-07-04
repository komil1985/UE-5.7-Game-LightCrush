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

UAudioComponent* UkdAudioSubsystem::SpawnPersistentDeck(USoundBase* WithSound)
{
    // CRITICAL: SpawnSound2D and CreateSound2D both return nullptr immediately
    // when Sound==nullptr — this was the root cause of all music failures.
    // We are always called with a real Track, but guard defensively.
    if (!WithSound) return nullptr;

    // CreateSound2D creates the component WITHOUT calling Play() — we drive
    // playback ourselves via FadeIn so we control the volume ramp.
    // SpawnSound2D would call Play() immediately at vol 0, which is wasteful
    // and bypasses our crossfade logic.
    UAudioComponent* Deck = UGameplayStatics::CreateSound2D(
        this,
        WithSound,
        /*VolumeMultiplier*/ 1.f,      // FadeIn owns the 0→target ramp via its internal
        // AdjustVolumeMultiplier. Base must be 1.f or the
        // multiply chain is permanently zeroed out.
        /*PitchMultiplier*/  1.f,
        /*StartTime*/        0.f,
        /*ConcurrencySettings*/ nullptr,
        /*bPersistAcrossLevelTransition*/ true,
        /*bAutoDestroy*/ false);

    if (Deck && BankA && BankA->MusicClass)
    {
        // Route through SC_Music so the Settings-menu Music slider governs volume.
        Deck->SoundClassOverride = BankA->MusicClass;
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("[kdAudio] SpawnPersistentDeck — %s for '%s'"),
        IsValid(Deck) ? TEXT("OK") : TEXT("FAILED"),
        *GetNameSafe(WithSound));
#endif

    return Deck;
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
    if (!Track)
    {
        UE_LOG(LogTemp, Error, TEXT("[kdAudio] RequestMusic — Track is NULL. DA field not assigned?"));
        return;
    }

    // Same track already playing → let it ride (seamless same-track level loads).
    if (CurrentTrack == Track && IsValid(ActiveMusic()) && ActiveMusic()->IsPlaying())
    {
        return;
    }

    const float Fade = (FadeSeconds >= 0.f)
        ? FadeSeconds
        : (BankA ? BankA->MusicCrossfadeSeconds : 1.5f);

    const float TargetVol = (bCrushFilterActive && BankA && BankA->bDuckMusicInCrush)
        ? BankA->CrushMusicVolume
        : 1.f;

    UAudioComponent* Outgoing = ActiveMusic(); // null on very first call — handled below

    // ── Incoming deck ─────────────────────────────────────────────────────────
    // Reuse the inactive slot if it's already a live component; otherwise spawn
    // a fresh one with the real Track. This is lazy — decks only exist once music
    // has actually been requested, which is why SpawnPersistentDeck(nullptr) was
    // always the wrong design (CreateSound2D rejects null sounds).
    UAudioComponent* Incoming = InactiveMusic();

    if (!IsValid(Incoming))
    {
        // First use of this slot — spawn with the real track so CreateSound2D
        // gets a non-null sound and actually creates the component.
        Incoming = SpawnPersistentDeck(Track);

        // Store back into the correct slot.
        if (ActiveDeck == 0) { MusicDeckB = Incoming; }
        else { MusicDeckA = Incoming; }
    }
    else
    {
        // Slot already exists (second or later crossfade). Stop any residual
        // playback from a previous fade-out before assigning the new sound —
        // SetSound on a playing/fading component can cause a brief pop.
        Incoming->Stop();
        Incoming->SetSound(Track);
    }

    if (!IsValid(Incoming))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[kdAudio] RequestMusic — deck spawn failed for '%s'. No audio device?"), *Track->GetName());
        return;
    }

    // ── Crossfade ─────────────────────────────────────────────────────────────
    Incoming->SetLowPassFilterEnabled(false);
    Incoming->FadeIn(Fade, TargetVol, 0.f);

    if (IsValid(Outgoing) && Outgoing->IsPlaying())
    {
        Outgoing->FadeOut(Fade, 0.f);
    }

    // Inherit crush muffle if we're already in shadow at the moment of request.
    if (bCrushFilterActive)
    {
        ApplyCrushFilterToDeck(Incoming, true, /*bInstant*/ true);
    }

    ActiveDeck = 1 - ActiveDeck;
    CurrentTrack = Track;

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("[kdAudio] RequestMusic — crossfade → '%s' (fade=%.2fs, vol=%.2f)"),
        *Track->GetName(), Fade, TargetVol);
#endif
}

void UkdAudioSubsystem::RequestMusicForLevel(FName LevelName)
{
    //if (!BankA) return;

    //USoundBase* Track = nullptr;
    //if (const TObjectPtr<USoundBase>* Found = BankA->PerLevelMusic.Find(LevelName))
    //{
    //    Track = *Found;
    //}
    //if (!Track)
    //{
    //    Track = BankA->DefaultGameplayMusic;
    //}
    //RequestMusic(Track);

    if (!BankA)
    {
        UE_LOG(LogTemp, Error, TEXT("[kdAudio] RequestMusicForLevel('%s') — BankA is NULL."),
            *LevelName.ToString());
        return;
    }

    // ── Resolution policy ─────────────────────────────────────────────────────
    //  1. PerLevelMusic entry exists with a track  → play it.
    //  2. PerLevelMusic entry exists but is None   → EXPLICIT SILENCE. Stop music.
    //     (An authored None is a decision, not an omission — do NOT fall back.)
    //  3. No entry at all                          → DefaultGameplayMusic.
    //  4. No entry and no default                  → stop music.
    //
    // Silence must always be reachable: the music deck persists across OpenLevel
    // (bPersistAcrossLevelTransition=true, required for crossfade continuity),
    // so "do nothing" here would let the previous level's track ride forever.

    USoundBase* Track = nullptr;
    const TCHAR* Source = TEXT("");

    if (const TObjectPtr<USoundBase>* Entry = BankA->PerLevelMusic.Find(LevelName))
    {
        if (*Entry)
        {
            Track = *Entry;
            Source = TEXT("PerLevelMusic");
        }
        else
        {
            UE_LOG(LogTemp, Log,
                TEXT("[kdAudio] Level '%s' → PerLevelMusic entry is None (explicit silence). Fading out."),
                *LevelName.ToString());
            StopMusic(BankA->MusicCrossfadeSeconds);
            return;
        }
    }
    else if (BankA->DefaultGameplayMusic)
    {
        Track = BankA->DefaultGameplayMusic;
        Source = TEXT("DefaultGameplayMusic (no PerLevelMusic entry)");
    }

    if (!Track)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[kdAudio] Level '%s' → no entry, no default. Fading out."),
            *LevelName.ToString());
        StopMusic(BankA->MusicCrossfadeSeconds);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[kdAudio] Level '%s' → '%s' via %s"),
        *LevelName.ToString(), *Track->GetName(), Source);
    RequestMusic(Track);
}

void UkdAudioSubsystem::RequestMenuMusic()
{
    if (!BankA) return;

    // Primary: dedicated MenuMusic field.
    USoundBase* Track = BankA->MenuMusic;

    // Fallback: inline PerLevelMusic["MainMenu"] lookup.
    // CRITICAL: do NOT call RequestMusicForLevel() here — that cross-call
    // creates a second entry point into RequestMusic during the PostLoadMapWithWorld
    // broadcast, which in UE5.7 causes infinite recursion via the persistent
    // audio component registration re-triggering the delegate.
    if (!Track)
    {
        static const FName MenuKey(TEXT("MainMenu"));
        if (const TObjectPtr<USoundBase>* Found = BankA->PerLevelMusic.Find(MenuKey))
        {
            Track = *Found;
        }
    }

    if (!Track)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[kdAudio] RequestMenuMusic — no track found. Assign DA_HeliographAudio.MenuMusic or PerLevelMusic[\"MainMenu\"]."));
        return;
    }

    RequestMusic(Track);
}

void UkdAudioSubsystem::StopMusic(float FadeSeconds)
{
    //if (IsValid(MusicDeckA) && MusicDeckA->IsPlaying()) { MusicDeckA->FadeOut(FadeSeconds, 0.f); }
    //if (IsValid(MusicDeckB) && MusicDeckB->IsPlaying()) { MusicDeckB->FadeOut(FadeSeconds, 0.f); }
    //CurrentTrack = nullptr;

    UAudioComponent* Active = ActiveMusic();
    if (IsValid(Active) && Active->IsPlaying())
    {
        Active->FadeOut(FMath::Max(0.f, FadeSeconds), 0.f);
    }

    // Clearing CurrentTrack is mandatory: the same-track early-out in
    // RequestMusic compares against it, and after an authored silence the
    // next request for the same track must be treated as a fresh start.
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
