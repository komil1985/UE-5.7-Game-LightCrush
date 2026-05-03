// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdCrushTransitionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrushTransitionComplete, bool, bIsCrushMode);

class UCurveFloat;
class AkdMyPlayer;
class UTimelineComponent;

// ─────────────────────────────────────────────────────────────────────────────
// UkdCrushTransitionComponent — TELEPHOTO PERSPECTIVE REWRITE
//
// The orthographic switch is gone entirely.  The camera stays in Perspective
// mode for its whole lifetime.  The "2D" look is achieved through a telephoto
// lens: extreme arm extension + narrow FOV.  At 14° FOV from 2200 cm away the
// projection rays are nearly parallel — indistinguishable from orthographic to
// the eye, but with real depth cues and zero projection-switch bugs.
//
// SINGLE TIMELINE architecture:
//   TotalDuration = AnticipationDuration + TransitionDuration
//
//   t = [0 … AnticipationRatio]
//     Camera begins moving immediately (arm extends, FOV narrows, pitch flattens).
//     Player is frozen.  Scale punch is applied.
//     Player X is snapped to CrushWorldX (the actual "crush").
//
//   t = [AnticipationRatio … 1]
//     Movement re-enabled.  Player mesh scale lerps to target.
//     Camera continues its morph in sync.
//     Mid-point: spring arm yaw locks / unlocks, bInheritYaw toggled.
//     Subtle camera roll peaks at mid-point and returns to zero — sells
//     the physical sensation of the world pivoting flat.
//
//   t = 1
//     All values snapped to exact targets.
//     OnTransitionComplete broadcast (gameplay state).
//     Settle overshoot: scale pops past target then decays via exp curve.
//
// BEAUTIFUL IN 2D MODE:
//   • 14° FOV + 2200 cm arm = diorama/miniature world look.
//   • Depth of field enabled: player is razor-sharp, far geometry blurs.
//   • Slight saturation reduction: world looks graphic, not dull.
//   • These are all native camera PP settings — zero extra materials needed.
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdCrushTransitionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UkdCrushTransitionComponent();

    UFUNCTION(BlueprintCallable, Category = "Crush | Transition")
    void StartTransition(bool bToCrushMode);

    UPROPERTY(BlueprintAssignable, Category = "Crush | Transition")
    FOnCrushTransitionComplete OnTransitionComplete;

    // ── Core timing ───────────────────────────────────────────────────────────

    /** Seconds the player is frozen + camera begins moving before the mesh morphs. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition",
        meta = (ClampMin = "0.0", ClampMax = "0.3"))
    float AnticipationDuration = 0.08f;

    /** Seconds for the full camera + mesh morph after anticipation. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition",
        meta = (ClampMin = "0.1"))
    float TransitionDuration = 0.55f;

    // ── Player mesh ───────────────────────────────────────────────────────────

    /** Player mesh scale in Crush (2D) mode.  3D scale is captured at BeginPlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Transition")
    FVector PlayerCrushScale = FVector(0.2f, 0.2f, 0.2f);

    /** Fraction past target to overshoot on landing. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition",
        meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float SettleOvershootAmount = 0.09f;

    /** Seconds for the settle overshoot to decay. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition",
        meta = (ClampMin = "0.0", ClampMax = "0.8"))
    float SettleDuration = 0.18f;

    /** Uniform scale-up fraction during anticipation (e.g. 0.12 = 112 %).
     *  Entering crush → UP (breath in).  Exiting → DOWN at half amount. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition",
        meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float AnticipationScalePunch = 0.12f;

    // ── World X ───────────────────────────────────────────────────────────────

    /** World X to snap the player to when entering Crush.
     *  Must match your shadow plane / geometry CrushWorldX.  Usually 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Transition")
    float CrushWorldX = 0.f;

    // ── Camera — 2D Crush targets (telephoto) ─────────────────────────────────

    /** Field of view in Crush (2D) mode.  12–18° gives a beautiful diorama look.
     *  3D FOV is captured from the camera at BeginPlay. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera",
        meta = (ClampMin = "5.0", ClampMax = "40.0"))
    float Crush2DFOV = 14.f;

    /** Spring arm length in Crush (2D) mode (cm).
     *  3D arm length is captured at BeginPlay. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera",
        meta = (ClampMin = "500.0"))
    float Crush2DArmLength = 2200.f;

    /** Spring arm relative rotation in Crush (2D) mode.
     *  Pitch 0 = camera looks straight horizontal (perfect side-on view).
     *  Yaw matches your shadow plane orientation. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera")
    FRotator Crush2DArmRotation = FRotator(0.f, 0.f, 0.f);

    /** Peak camera roll at mid-transition (degrees).
     *  A gentle ±2–4° sell the physical sensation of the world pivoting flat.
     *  Set to 0 to disable. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera",
        meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float TransitionRollDegrees = 3.f;

    // ── Camera — 2D Depth of Field (diorama look) ─────────────────────────────

    /** Enable cinematic DOF while in Crush mode.
     *  Player stays sharp, far geometry softens — the diorama effect. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera | DOF")
    bool bEnableCrushDOF = true;

    /** Camera f-stop in Crush mode (lower = more blur, 1.4–2.8 for diorama). */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera | DOF",
        meta = (ClampMin = "0.5", ClampMax = "22.0"))
    float Crush2DFStop = 2.0f;

    /** Simulated sensor width (mm) used with Crush f-stop for blur calculation. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera | DOF",
        meta = (ClampMin = "1.0"))
    float Crush2DSensorWidth = 150.f;

    // ── Camera — 2D Color grading ─────────────────────────────────────────────

    /** Saturation multiplier in Crush (2D) mode.
     *  0.80–0.88 makes the world look slightly flat and graphic. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera | Color",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Crush2DSaturation = 0.82f;

    /** Contrast multiplier in Crush (2D) mode.  1.05–1.12 crispens shadows. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera | Color",
        meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float Crush2DContrast = 1.08f;

    /** Speed at which DOF and color grading lerp in / out. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Camera | Color",
        meta = (ClampMin = "0.5"))
    float CameraGradeLerpSpeed = 5.f;

protected:
    virtual void BeginPlay() override;

private:
    // ── Timeline ──────────────────────────────────────────────────────────────
    UPROPERTY()
    TObjectPtr<UTimelineComponent> CrushTimeline;

    /** Optional ease curve (0→1 over normalised time).  Linear if null. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition")
    TObjectPtr<UCurveFloat> TransitionCurve;

    UFUNCTION() void HandleTimelineUpdate(float Value);
    UFUNCTION() void HandleTimelineFinished();

    UPROPERTY()
    TObjectPtr<AkdMyPlayer> CachedOwner;

    // ── Per-transition state ──────────────────────────────────────────────────
    bool  bTargetCrushMode = false;
    bool  bYawLocked = false;   // set at midpoint, guards single-fire
    bool  bMovementRestored = false;   // set when AnticipationRatio is passed
    float AnticipationRatio = 0.f;     // = AnticipationDuration / TotalDuration

    // Mesh lerp
    FVector MeshScaleFrom;
    FVector MeshScaleTo;

    // Camera lerp
    float   FOVFrom = 90.f;
    float   FOVTo = 90.f;
    float   ArmLengthFrom = 300.f;
    float   ArmLengthTo = 300.f;
    FRotator ArmRotFrom = FRotator(-30.f, 0.f, 0.f);
    FRotator ArmRotTo = FRotator(0.f, 0.f, 0.f);

    // ── Stable baselines — captured once at BeginPlay ─────────────────────────
    FVector  Original3DScale = FVector::OneVector;
    float    Original3DFOV = 90.f;
    float    Original3DArmLength = 300.f;
    FRotator Original3DArmRotation = FRotator(-30.f, 0.f, 0.f);

    // ── Settle ────────────────────────────────────────────────────────────────
    FTimerHandle SettleTickHandle;
    float        SettleElapsed = 0.f;
    void         SettleTick();

    // ── Camera helpers ────────────────────────────────────────────────────────
    void ApplyCameraState(float FOV, float ArmLen, const FRotator& ArmRot, float Roll);
    void RestoreCameraDefaults();
};
