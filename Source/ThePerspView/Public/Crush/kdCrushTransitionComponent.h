// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdCrushTransitionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrushTransitionComplete, bool, bIsCrushMode);



class UCurveFloat;
class AkdMyPlayer;
class UTimelineComponent;
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdCrushTransitionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UkdCrushTransitionComponent();

    UFUNCTION(BlueprintCallable, Category = "Crush | Transition")
    void StartTransition(bool bToCrushMode);

    /** Called by CrushStateComponent after floor trace places the player safely.
    *  Removes the X plane constraint that prevents falling during transition. */
    UFUNCTION(BlueprintCallable, Category = "Crush | Transition")
    void ReleasePlaneConstraint();

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

    // ── Z Squash-Stretch — Entering Crush ─────────────────────────────────────

    /** Upward mesh offset during anticipation when entering crush (cm).
    *  The character rises before being pressed flat.  Pure visual — no collision. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Polish | Enter Z",
        meta = (ClampMin = "0.0", ClampMax = "120.0"))
    float EnterStretchZ = 35.f;

    /** How much the Z scale stretches during the rise (fraction of base scale).
     *  e.g. 0.25 = 125% tall at peak.  Returns to normal as morph begins. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Polish | Enter Z",
        meta = (ClampMin = "0.0", ClampMax = "0.6"))
    float EnterStretchScaleZ = 0.25f;

    /** Downward offset at morph end — character sinks below origin before
     *  the scale morph presses them fully flat (cm).  Reads as weight. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Polish | Enter Z",
        meta = (ClampMin = "0.0", ClampMax = "60.0"))
    float EnterSinkZ = 12.f;

    // ── Z Squash-Stretch — Exiting Crush ─────────────────────────────────────

    /** Downward dip during anticipation when exiting crush (cm).
     *  The character compresses before ejecting.  Reads as spring coiling. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Polish | Exit Z",
        meta = (ClampMin = "0.0", ClampMax = "40.0"))
    float ExitDipZ = 10.f;

    /** Squash on Z scale during the dip. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Polish | Exit Z",
        meta = (ClampMin = "0.0", ClampMax = "0.4"))
    float ExitDipSquashZ = 0.12f;

    /** Peak upward offset after exiting (cm) — the ejection pop.
     *  Reaches this value at ~60% of the morph phase, then returns to 0. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Polish | Exit Z",
        meta = (ClampMin = "0.0", ClampMax = "150.0"))
    float ExitPopZ = 55.f;

    /** Z stretch fraction at the pop peak. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Polish | Exit Z",
        meta = (ClampMin = "0.0", ClampMax = "0.6"))
    float ExitPopStretchZ = 0.30f;


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
    FVector OriginalMeshRelativeLoc = FVector::ZeroVector;  // captured at BeginPlay

    // Camera lerp
    float   FOVFrom = 90.f;
    float   FOVTo = 90.f;
    float   ArmLengthFrom = 300.f;
    float   ArmLengthTo = 300.f;
    FRotator ArmRotFrom = FRotator(-30.f, 0.f, 0.f);
    FRotator ArmRotTo = FRotator(0.f, 0.f, 0.f);

    FQuat ArmRotFromQ;
    FQuat ArmRotToQ;

    // ── Stable baselines — captured once at BeginPlay ─────────────────────────
    FVector  Original3DScale = FVector::OneVector;
    float    Original3DFOV = 90.f;
    float    Original3DArmLength = 300.f;
    FRotator Original3DArmRotation = FRotator(-30.f, 0.f, 0.f);
    FQuat   Original3DArmRotQ = FQuat::Identity;

    // ── Settle ────────────────────────────────────────────────────────────────
    FTimerHandle SettleTickHandle;
    float        SettleElapsed = 0.f;
    void         SettleTick();

    // ── Player world-X lerp ───────────────────────────────────────────────────
    // Captured at the START of each transition so mid-cancel is handled cleanly.
    float PlayerXFrom = 0.f;
    float PlayerXTo = 0.f;

    // Saved when ENTERING crush so we know where to return on exit.
    float CachedPreCrushX = 0.f;

    // ── Camera helpers ────────────────────────────────────────────────────────
    void ApplyCameraState(float FOV, float ArmLen, const FRotator& ArmRot, float Roll);
    void RestoreCameraDefaults();

    // ── Z helpers ─────────────────────────────────────────────────────────────

    /** Smoothstep curve: 0→1 mapped through 3t²-2t³.
    *  Gives zero first-derivative at both ends — no velocity pop. */
    static FORCEINLINE float Smooth(float t)
    {
        t = FMath::Clamp(t, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    void ApplyZSquashStretch(float Alpha, USkeletalMeshComponent* Mesh);

    // ── Plane constraint ──────────────────────────────────────────────────────
    void ApplyPlaneConstraint();
    void RemovePlaneConstraintInternal();
};
