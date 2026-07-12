// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "kdTutorialBank.generated.h"

class UInputAction;
class USoundBase;

/**
 * How a tutorial step decides it has been learned.
 *
 * SEMANTICS OF THE "ALREADY SATISFIED" CASE:
 *   TagAdded   — if the tag is ALREADY present when the step arms, the lesson is
 *                moot; the step is silently retired and marked seen.
 *   TagRemoved — mirror image: already absent at arm time ⇒ silently retired.
 *   Author steps so the pre-condition is the natural state on entry.
 */
UENUM(BlueprintType)
enum class EkdTutorialCompletion : uint8
{
    Timer       UMETA(DisplayName = "Timer (auto-dismiss)"),
    TagAdded    UMETA(DisplayName = "Gameplay Tag Added"),
    TagRemoved  UMETA(DisplayName = "Gameplay Tag Removed"),
    PawnMoved   UMETA(DisplayName = "Pawn Exceeded Speed"),
    ExitVolume  UMETA(DisplayName = "Player Left Source Trigger"),
    Manual      UMETA(DisplayName = "Manual (code-driven)")
};

/** One authored lesson. Copy + condition + pacing live together. */
USTRUCT(BlueprintType)
struct THEPERSPVIEW_API FkdTutorialStep
{
    GENERATED_BODY()

    /** Unique. Referenced by AkdTutorialTrigger and by code hooks. e.g. "Tut.Crush" */
    UPROPERTY(EditAnywhere, Category = "Identity")
    FName StepId = NAME_None;

    UPROPERTY(EditAnywhere, Category = "Copy")
    FText Headline;

    UPROPERTY(EditAnywhere, Category = "Copy", meta = (MultiLine = "true"))
    FText Body;

    /** Optional. The bound key is resolved at runtime, so rebinds never desync the copy. */
    UPROPERTY(EditAnywhere, Category = "Copy")
    TObjectPtr<UInputAction> PromptAction = nullptr;

    UPROPERTY(EditAnywhere, Category = "Completion")
    EkdTutorialCompletion Completion = EkdTutorialCompletion::Timer;

    /** Used by TagAdded / TagRemoved. */
    UPROPERTY(EditAnywhere, Category = "Completion")
    FGameplayTag CompletionTag;

    /** Used by Timer: seconds on screen before auto-dismiss. */
    UPROPERTY(EditAnywhere, Category = "Completion", meta = (ClampMin = "0.1"))
    float TimerSeconds = 4.0f;

    /** Used by PawnMoved: cm/s the pawn must exceed. */
    UPROPERTY(EditAnywhere, Category = "Completion", meta = (ClampMin = "0.0"))
    float MovedSpeedThreshold = 150.0f;

    /**
     * ADAPTIVE SILENCE. Grace period between arming and revealing.
     * Solve the lesson inside this window and the prompt never appears.
     * 0 = reveal immediately (use for danger callouts, e.g. "you are burning").
     */
    UPROPERTY(EditAnywhere, Category = "Pacing", meta = (ClampMin = "0.0"))
    float HintDelaySeconds = 2.0f;

    /** Minimum on-screen time once revealed. Prevents a one-frame flicker. */
    UPROPERTY(EditAnywhere, Category = "Pacing", meta = (ClampMin = "0.0"))
    float MinDisplaySeconds = 1.25f;

    /** If true, never shown again on this save once learned. */
    UPROPERTY(EditAnywhere, Category = "Persistence")
    bool bShowOncePerSave = true;

    /** If the player's ASC owns ANY of these when the step arms, drop it silently. */
    UPROPERTY(EditAnywhere, Category = "Gating")
    FGameplayTagContainer SuppressIfAnyTagPresent;

    /**
     * Lock tag removed the instant this step's trigger is entered. This is the
     * "unlock" — granted BEFORE the seen-check, so a returning player (hints
     * suppressed) still regains the action. Leave invalid for steps that gate
     * nothing (e.g. Tut.Goal).
     */
    UPROPERTY(EditAnywhere, Category = "Gating")
    FGameplayTag UnlockTag;

    /** Routed through UkdAudioSubsystem::PlaySFX2D. Never played directly. */
    UPROPERTY(EditAnywhere, Category = "Feedback")
    TObjectPtr<USoundBase> RevealSound = nullptr;
};

/**
 * UkdTutorialBank — the DA_Heliograph of teaching.
 * Single source of truth for every lesson. Mirrors DA_HeliographAudio's role.
 */
UCLASS(BlueprintType)
class THEPERSPVIEW_API UkdTutorialBank : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Tutorial", meta = (TitleProperty = "StepId"))
    TArray<FkdTutorialStep> Steps;

    /** O(1). Returns nullptr if unknown. Pointer is stable for the asset's lifetime. */
    const FkdTutorialStep* FindStep(FName StepId) const;

    virtual void PostLoad() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    void RebuildIndex();

    /** Transient: index into Steps. Never serialized. */
    TMap<FName, int32> StepIndex;
};