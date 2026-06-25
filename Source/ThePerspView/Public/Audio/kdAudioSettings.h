// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "kdAudioSettings.generated.h"


class UkdAudioBank;

// ─────────────────────────────────────────────────────────────────────────────
// UkdAudioSettings — project-level pointer to the active UkdAudioBank.
//
// Mirrors UkdThemeSettings exactly: it lets UkdAudioSubsystem resolve the bank
// from Project Settings without depending on a live player pawn, which is
// essential because music has to start on the menu (and on level BeginPlay)
// before any pawn is guaranteed to exist.
//
// SETUP:  Project Settings → Game → Heliograph Audio → Audio Bank = DA_HeliographAudio
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Heliograph Audio"))
class THEPERSPVIEW_API UkdAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
    /** Settings appear under the "Game" category, next to your theme settings. */
    virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

    /** The bank the audio subsystem plays from. Soft ref so it loads on demand. */
    UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (DisplayName = "Audio Bank"))
    TSoftObjectPtr<UkdAudioBank> AudioBank;
};
