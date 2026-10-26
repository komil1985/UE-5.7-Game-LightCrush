// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Camera/CameraTypes.h"
#include "kdLevelIntroSettings.generated.h"

/**
 * Project Settings -> Game -> kd Level Intro.
 * Tunables for UkdLevelIntroSubsystem (goal reveal at level start).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "kd Level Intro"))
class THEPERSPVIEW_API UkdLevelIntroSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

    /** Master switch. */
    UPROPERTY(config, EditAnywhere, Category = "Intro")
    bool bEnabled = true;

    /** Seconds the camera rests on the goal before flying back to the player. */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Timing", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float HoldSeconds = 2.0f;

    /** Seconds for the fly-back blend from goal to player. */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Timing", meta = (ClampMin = "0.1", ClampMax = "6.0"))
    float BlendSeconds = 2.0f;

    /** Blend duration used when the player skips the hold (shorter, snappy). */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Timing", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float SkipBlendSeconds = 0.6f;

    /** Black -> clear fade at the very start, hides the level-load hitch. 0 = off. */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Timing", meta = (ClampMin = "0.0", ClampMax = "3.0"))
    float FadeInSeconds = 0.5f;

    /** Blend curve. EaseInOut reads best for a "fly home" move. */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Blend")
    TEnumAsByte<EViewTargetBlendFunction> BlendFunction = VTBlend_EaseInOut;

    /** Exponent for the Ease* blend functions (2-3 is gentle, higher = sharper). */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Blend", meta = (ClampMin = "1.0", ClampMax = "6.0"))
    float BlendExp = 2.5f;

    /** World-space offset from the goal's origin to the point the camera frames. */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Framing")
    FVector GoalFocusOffset = FVector(0.f, 0.f, 60.f);

    /** Radius of the sphere swept from the goal toward the camera so walls can't
     *  swallow it (same idea as the spring arm's ProbeSize). */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Framing", meta = (ClampMin = "1.0"))
    float ProbeRadius = 12.f;

    /** Slow push-in toward the goal during the hold (cm). 0 = static. */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Framing", meta = (ClampMin = "0.0", ClampMax = "600.0"))
    float HoldDriftCm = 120.f;

    /** Any key skips the hold. */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Skip")
    bool bAllowSkip = true;

    /** Ignore skip presses for this long after the intro starts (stray menu clicks). */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Skip", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float SkipGraceSeconds = 0.4f;

    /** false = plays every time the level loads (including Retry / Restart).
     *  true  = plays once per level per game session (Retry skips it). */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Rules")
    bool bOncePerSession = false;

    /** Never play in the tutorial map (its prompts/locks start immediately). */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Rules")
    bool bSkipInTutorial = true;

    /** Extra maps to exclude, by map name (e.g. "L_Level_05"). */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Rules")
    TArray<FName> ExcludedLevels;

    /** Fire a Heliograph edge pulse through UkdWorldColorDriver when the camera
     *  starts flying home. Routed via the driver, so single-writer is respected. */
    UPROPERTY(config, EditAnywhere, Category = "Intro|Polish")
    bool bPulseOnReveal = false;

    UPROPERTY(config, EditAnywhere, Category = "Intro|Polish", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bPulseOnReveal"))
    float RevealPulseStrength = 0.6f;
};