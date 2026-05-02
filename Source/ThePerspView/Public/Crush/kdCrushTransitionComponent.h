// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdCrushTransitionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrushTransitionComplete, bool, bIsCrushMode);


// ─────────────────────────────────────────────────────────────────────────────
// UkdCrushTransitionComponent
//
// Drives the 3D ↔ 2D (Crush Mode) visual transition in three distinct phases:
//
//   1. ANTICIPATION  (AnticipationDuration seconds)
//      A brief scale punch fired immediately so the player knows input landed.
//      Entering Crush: player briefly scales UP (breath in before squashing flat).
//      Exiting  Crush: player briefly scales DOWN (compress before expanding out).
//
//   2. MAIN MORPH  (TransitionDuration seconds, driven by CrushTimeline)
//      Lerps mesh scale and camera OrthoWidth from current to target.
//      Switches projection mode exactly once at the 0.5 alpha midpoint.
//
//   3. SETTLE  (SettleDuration seconds, timer-driven, purely visual)
//      After the morph the scale overshoots by SettleOvershootAmount then
//      exponentially decays back to target — a satisfying "pop" landing.
//      OnTransitionComplete is broadcast BEFORE the settle so gameplay state
//      updates happen without delay.
//
// SETUP:
//   1. Attach to AkdMyPlayer in the constructor.
//   2. Assign TransitionCurve (UCurveFloat, 0 → 1 over normalised time).
//   3. Tune all Feel properties in the Blueprint defaults.
//   4. The 3D mesh scale and 3D OrthoWidth are captured at BeginPlay —
//      do not set them by hand.
// ─────────────────────────────────────────────────────────────────────────────


class UCurveFloat;
class AkdMyPlayer;
class UTimelineComponent;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdCrushTransitionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdCrushTransitionComponent();

	UFUNCTION(BlueprintCallable, Category = "Crush | System")
	void StartTransition(bool bToCrushMode);

	UPROPERTY(BlueprintAssignable, Category = "Crush | System")
	FOnCrushTransitionComplete OnTransitionComplete;

	/* -- Configuration -- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Settings | Transition", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float TransitionDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Settings | Transition")
	FVector PlayerCrushScale = FVector(0.2f, 0.2f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Settings | Transition")
	FVector PlayerOriginalScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Settings | Transition")
	float CrushOrthoWidth = 1500.0f;

	// ── Feel — Anticipation ───────────────────────────────────────────────────

	/** How long the scale punch holds before the main morph begins (seconds).
	 *  Set to 0 to disable anticipation entirely. */
	UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition | Feel", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float AnticipationDuration = 0.08f;

	/** Uniform scale fraction added during anticipation.
	 *  Entering Crush → mesh scales UP by this amount   (e.g. 0.12 = 112 %).
	 *  Exiting  Crush → mesh scales DOWN by half amount (e.g. 0.06 = 94 %). */
	UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition | Feel", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float AnticipationScalePunch = 0.12f;

	// ── Feel — Settle (post-morph overshoot) ─────────────────────────────────

	/** Fraction past the target scale to overshoot on landing.
	 *  e.g. 0.10 → target × 1.10, then decays back.  Set to 0 to disable. */
	UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition | Feel",	meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float SettleOvershootAmount = 0.10f;

	/** Seconds for the overshoot to exponentially decay back to target scale. */
	UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition | Feel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SettleDuration = 0.18f;

protected:
	virtual void BeginPlay() override;
	
private:
	// ── Timeline plumbing ─────────────────────────────────────────────────────

	UPROPERTY()
	TObjectPtr<UTimelineComponent> CrushTimeline;

	UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition")
	TObjectPtr<UCurveFloat> TransitionCurve;

	UFUNCTION()
	void HandleTimelineUpdate(float Value);

	UFUNCTION()
	void HandleTimelineFinished();

	// ── Cached reference ──────────────────────────────────────────────────────

	UPROPERTY()
	TObjectPtr<AkdMyPlayer> CachedOwner;

	// ── Per-transition state ──────────────────────────────────────────────────

	bool    bTargetCrushMode = false;
	bool    bProjectionSwitchDone = false;

	FVector InitialScale;
	float   InitialOrthoWidth = 0.f;

	FVector TargetScale;
	float   TargetOrthoWidth = 0.f;

	// ── Stable baselines captured once at BeginPlay ───────────────────────────

	FVector  Original3DScale = FVector::OneVector;
	float    Original3DOrthoWidth = 512.f;
	FRotator Original3DArmRotation = FRotator(-30.f, 0.f, 0.f);
	FRotator Crush2DArmRotation = FRotator::ZeroRotator;

	// ── Anticipation ──────────────────────────────────────────────────────────

	FTimerHandle AnticipationTimerHandle;

	/** Called after AnticipationDuration elapses — starts the main morph. */
	void BeginMainTimeline();

	// ── Settle ────────────────────────────────────────────────────────────────

	FTimerHandle SettleTickHandle;
	float        SettleElapsed = 0.f;

	/** 60 Hz looping tick — decays scale from the overshoot back to TargetScale. */
	void SettleTick();
};
