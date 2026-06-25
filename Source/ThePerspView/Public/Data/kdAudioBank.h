// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "kdAudioBank.generated.h"

class USoundBase;
class USoundClass;

// ─────────────────────────────────────────────────────────────────────────────
// UkdAudioBank — the project's "sonic theme".
//
// This is the audio counterpart to DA_Heliograph (UkdColorTheme): one swappable
// DataAsset that holds every sound the game plays, so the whole soundscape can
// be re-skinned by assigning a different bank. UkdAudioSubsystem reads it and is
// the only thing that triggers playback — no actor plays bank sounds directly.
//
// SETUP:
//   1. Create a DataAsset of this class:  DA_HeliographAudio.
//   2. Assign it in Project Settings → Game → Heliograph Audio (UkdAudioSettings).
//   3. Every MUSIC cue must be marked Looping in the Sound Cue / Sound Wave.
//   4. Assign SC_Music / SC_SFX so the existing volume sliders route correctly.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class THEPERSPVIEW_API UkdAudioBank : public UDataAsset
{
	GENERATED_BODY()
	
public:
    // ── Sound class routing ───────────────────────────────────────────────────
    // Assign the same SoundClasses you hand to UkdGameInstance (SC_Music / SC_SFX).
    // The subsystem stamps these as the SoundClassOverride on each music deck so
    // the Master→Music / Master→SFX volume sliders in your Settings menu keep
    // working untouched. Category volume = SoundMix (GameInstance owns it).
    // Per-track crossfade + crush duck = AudioSubsystem owns it. No overlap.

    UPROPERTY(EditDefaultsOnly, Category = "Routing")
    TObjectPtr<USoundClass> MusicClass;

    UPROPERTY(EditDefaultsOnly, Category = "Routing")
    TObjectPtr<USoundClass> SFXClass;

    // ── Music ─────────────────────────────────────────────────────────────────

    /** Played by AkdMainMenuGameMode on the menu level. Must loop. */
    UPROPERTY(EditDefaultsOnly, Category = "Music")
    TObjectPtr<USoundBase> MenuMusic;

    /** Fallback gameplay track when a level isn't in PerLevelMusic. Must loop. */
    UPROPERTY(EditDefaultsOnly, Category = "Music")
    TObjectPtr<USoundBase> DefaultGameplayMusic;

    /** Per-level overrides keyed by the level's FName (e.g. "L_Level_03").
     *  Lets each of your five levels carry its own theme without any BP logic. */
    UPROPERTY(EditDefaultsOnly, Category = "Music")
    TMap<FName, TObjectPtr<USoundBase>> PerLevelMusic;

    /** Optional sting layered once when a level is completed. */
    UPROPERTY(EditDefaultsOnly, Category = "Music")
    TObjectPtr<USoundBase> LevelCompleteSting;

    /** Crossfade time (seconds) between any two tracks. */
    UPROPERTY(EditDefaultsOnly, Category = "Music", meta = (ClampMin = "0.0", ClampMax = "8.0"))
    float MusicCrossfadeSeconds = 1.5f;

    // ── Crush-mode music duck (the "into the shadow" muffle) ──────────────────

    /** Muffle + lower the music while in Crush Mode for an underwater/ink feel. */
    UPROPERTY(EditDefaultsOnly, Category = "Music|Crush Duck")
    bool bDuckMusicInCrush = true;

    /** Low-pass cutoff applied to music in Crush Mode (Hz). Lower = more muffled. */
    UPROPERTY(EditDefaultsOnly, Category = "Music|Crush Duck", meta = (ClampMin = "200.0", ClampMax = "20000.0"))
    float CrushMusicLowPassHz = 1200.f;

    /** Music volume multiplier while in Crush Mode (1 = no change). */
    UPROPERTY(EditDefaultsOnly, Category = "Music|Crush Duck", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrushMusicVolume = 0.70f;

    /** Seconds for the duck (and its release) to ramp. Keep snappy. */
    UPROPERTY(EditDefaultsOnly, Category = "Music|Crush Duck", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrushDuckRamp = 0.20f;

    // ── Crush-mode signature SFX ──────────────────────────────────────────────
    // These are the most important sounds in the build — the toggle is the game.

    /** Whoosh on the frame the player enters Crush (input registered). */
    UPROPERTY(EditDefaultsOnly, Category = "SFX|Crush")
    TObjectPtr<USoundBase> CrushEnterStart;

    /** Whoosh on the frame the player exits Crush back to 3D. */
    UPROPERTY(EditDefaultsOnly, Category = "SFX|Crush")
    TObjectPtr<USoundBase> CrushExitStart;

    /** The "thud" the moment the morph lands (transition finished). */
    UPROPERTY(EditDefaultsOnly, Category = "SFX|Crush")
    TObjectPtr<USoundBase> CrushLand;

    /** Soft denial blip when the camera isn't aligned to a cardinal axis. */
    UPROPERTY(EditDefaultsOnly, Category = "SFX|Crush")
    TObjectPtr<USoundBase> CrushDenied;

    // ── Shadow SFX ────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, Category = "SFX|Shadow")
    TObjectPtr<USoundBase> ShadowEnter;

    UPROPERTY(EditDefaultsOnly, Category = "SFX|Shadow")
    TObjectPtr<USoundBase> ShadowExit;

    // ── UI SFX ────────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, Category = "SFX|UI")
    TObjectPtr<USoundBase> UIClick;

    UPROPERTY(EditDefaultsOnly, Category = "SFX|UI")
    TObjectPtr<USoundBase> UIBack;

    UPROPERTY(EditDefaultsOnly, Category = "SFX|UI")
    TObjectPtr<USoundBase> UIHover;

    // ── Stingers ──────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, Category = "SFX|Stinger")
    TObjectPtr<USoundBase> LevelComplete;

    UPROPERTY(EditDefaultsOnly, Category = "SFX|Stinger")
    TObjectPtr<USoundBase> Death;

    UPROPERTY(EditDefaultsOnly, Category = "SFX|Stinger")
    TObjectPtr<USoundBase> GameOver;
};
