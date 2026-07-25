// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Crush/kdCrushDirection.h"
#include "kdCrushAlignmentComponent.generated.h"

class AkdMyPlayer;
class APlayerController;
class UAbilitySystemComponent;
class USoundBase;

// ─────────────────────────────────────────────────────────────────────────────
// EkdRegisterState
//
// Three-band description of how close the camera is to a crush-legal cardinal.
// Deliberately coarse: the UI animates off the continuous Register01 value, and
// uses the state only to fire one-shot events (the plate click, the punch).
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EkdRegisterState : uint8
{
    /** Outside the capture arc, or crush is unavailable entirely. UI hidden. */
    Idle     UMETA(DisplayName = "Idle"),

    /** Inside the capture arc but outside the ability tolerance. UI fading in. */
    Seeking  UMETA(DisplayName = "Seeking"),

    /** Inside the ability tolerance — a crush press right now WILL succeed. */
    Locked   UMETA(DisplayName = "Locked")
};

// ─────────────────────────────────────────────────────────────────────────────
// EkdCrushIntentResult
//
// The answer this component gives AkdMyPlayer::RequestCrushToggle() when the
// player presses Crush while in 3D.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EkdCrushIntentResult : uint8
{
    /** Already aligned. Caller should activate the ability right now. */
    Immediate      UMETA(DisplayName = "Immediate"),

    /** Inside the capture arc. This component has taken ownership of the press
     *  and will re-call RequestCrushToggle() the frame alignment resolves.
     *  Caller must return without activating anything. */
    Buffered       UMETA(DisplayName = "Buffered"),

    /** Outside the capture arc. The denial cue has already been fired by this
     *  component. Caller must return without activating anything. */
    Denied         UMETA(DisplayName = "Denied"),

    /** This component has no opinion (disabled, already crushed, dead,
     *  transitioning...). Caller should run its normal unmodified path. */
    NotApplicable  UMETA(DisplayName = "Not Applicable")
};

/** Fired only on state CHANGE — safe to hang one-shot audio / animation off. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FkdOnRegisterStateChanged, EkdRegisterState, NewState, EkdCrushDirection, PredictedDirection);

/** Fired when a buffered intent finishes: true = auto-fired, false = timed out / cancelled. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FkdOnCrushIntentResolved, bool, bSucceeded);

/**
 * UkdCrushAlignmentComponent  —  "The Plate Register"
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Lives on AkdMyPlayer. Solves the single biggest friction point in Crush Mode:
 * the player cannot see the ±ToleranceDegrees cardinal cone that gates entry,
 * so a denied press feels like a broken button rather than a missed input.
 *
 * Three cooperating layers, in order of how much frustration each removes:
 *
 *   LAYER 1 — YAW DETENT (input feel)
 *     A proximity-weighted magnetic pull toward the nearest cardinal, active
 *     only inside a *capture arc* (default 2.5x the ability tolerance) and
 *     gated by how fast the player is actually turning. Whip the mouse and the
 *     assist vanishes entirely; make a fine adjustment and the camera settles
 *     onto the axis by itself. The player never fights the assist because the
 *     assist yields the instant they move with intent.
 *
 *   LAYER 2 — INTENT BUFFER (input contract)
 *     A press inside the capture arc but outside tolerance is no longer eaten.
 *     It is LATCHED: the component slews the camera hard to the exact cardinal
 *     and fires the crush itself the frame it resolves, within IntentBufferTime.
 *     Effective success window becomes the capture arc (~30 deg) instead of the
 *     tolerance (~12 deg) — a 2.5x larger target with ZERO loss of precision,
 *     because the slew lands on the cardinal exactly.
 *
 *   LAYER 3 — REGISTER READOUT (information)
 *     Publishes Register01 (0 at the arc edge, 1 at lock) plus a coarse state
 *     and the predicted collapse direction. UkdCrushRegisterWidget pulls these
 *     to draw a split-prism rangefinder that converges and clicks shut on lock.
 *     Invisible at rest — it only appears once the player is already hunting.
 *
 * ── OWNERSHIP CONTRACT (single-writer rule) ─────────────────────────────────
 *
 *   THIS COMPONENT IS THE SOLE WRITER OF APlayerController::ControlRotation.Yaw
 *   OUTSIDE OF CRUSH MODE, other than the player's own AddYawInput.
 *
 *   It writes nothing else. No MPC, no post-process, no mesh transform, no
 *   camera/spring-arm state. It goes fully inert (and writes NOTHING) whenever
 *   State.CrushMode, State.Transitioning, State.Dead or State.Exhausted is
 *   present, so UkdCrushTransitionComponent and UkdStrategicCameraComponent
 *   retain uncontested authority over the camera during their windows.
 *
 * ── TICK ORDERING ───────────────────────────────────────────────────────────
 *
 *   BeginPlay installs a tick prerequisite on the owning PlayerController, so
 *   this component always runs AFTER APlayerController::UpdateRotation has
 *   consumed the frame's look input. The camera manager updates inside the same
 *   controller tick, so an assist written here is presented on the NEXT frame.
 *   That one-frame latency is invisible: the assist is a continuous exponential
 *   slew, never a snap, so there is no discrete event to be late.
 *
 * ── TOLERANCE SOURCE OF TRUTH ───────────────────────────────────────────────
 *
 *   Tolerance is READ from the Ukd_CrushToggle CDO at BeginPlay, never
 *   duplicated. If a designer retunes CrushAlignmentToleranceDegrees in the
 *   ability Blueprint, this component and the HUD follow automatically. A
 *   hardcoded second copy would silently desync and make the readout lie.
 */
UCLASS(ClassGroup = (kd), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdCrushAlignmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UkdCrushAlignmentComponent();

    // ═══════════════════════════════════════════════════════════════════════
    // Public read API — everything downstream pulls from here.
    // ═══════════════════════════════════════════════════════════════════════

    /** 0 at the capture-arc edge, 1 when a crush press is guaranteed to succeed.
     *  Exactly 0 whenever crush entry is unavailable for any reason. */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Register")
    FORCEINLINE float GetRegister01() const { return Register01; }

    /** Coarse band. Use for one-shot cues; use Register01 for animation. */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Register")
    FORCEINLINE EkdRegisterState GetRegisterState() const { return RegisterState; }

    /** The direction the player WOULD collapse along if they pressed now.
     *  EkdCrushDirection::None when outside the capture arc. */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Register")
    FORCEINLINE EkdCrushDirection GetPredictedDirection() const { return PredictedDirection; }

    /** Absolute shortest-arc yaw error to the nearest cardinal, in degrees. */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Register")
    FORCEINLINE float GetAlignmentErrorDegrees() const { return AlignmentErrorDegrees; }

    /** The tolerance resolved from the Ukd_CrushToggle CDO (degrees). */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Register")
    FORCEINLINE float GetToleranceDegrees() const { return ResolvedToleranceDegrees; }

    /** Half-angle of the assist/buffer arc (degrees). Always > tolerance. */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Register")
    float GetCaptureArcDegrees() const;

    /** True while a press is latched and the camera is auto-slewing to lock. */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Register")
    FORCEINLINE bool IsIntentPending() const { return bIntentPending; }

    // ═══════════════════════════════════════════════════════════════════════
    // Public write API — exactly one entry point.
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Called by AkdMyPlayer::RequestCrushToggle() on the ENTER path only.
     * See EkdCrushIntentResult for the caller's obligations per return value.
     *
     * Idempotent while a buffer is live: a second press inside the window is
     * absorbed (returns Buffered again) rather than restarting the timer.
     */
    UFUNCTION(BlueprintCallable, Category = "kd|Crush|Register")
    EkdCrushIntentResult RequestCrushIntent();

    /** Drop any latched press without firing. Safe to call at any time.
     *  Call from death, level-complete, pause, cutscene entry. */
    UFUNCTION(BlueprintCallable, Category = "kd|Crush|Register")
    void CancelPendingIntent(bool bBroadcastFailure = true);

    // ═══════════════════════════════════════════════════════════════════════
    // Events
    // ═══════════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintAssignable, Category = "kd|Crush|Register")
    FkdOnRegisterStateChanged OnRegisterStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "kd|Crush|Register")
    FkdOnCrushIntentResolved OnCrushIntentResolved;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ═══════════════════════════════════════════════════════════════════════
    // LAYER 1 — Yaw detent tuning
    // ═══════════════════════════════════════════════════════════════════════

    /** Master switch for the magnetic pull. The readout (Layer 3) and the
     *  buffer (Layer 2) keep working with this off — useful as an
     *  accessibility / "purist" option in the settings menu. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Assist")
    bool bYawAssistEnabled = true;

    /** Capture arc = tolerance * this. The arc is where assist and buffering
     *  live. 2.5 over a 12 deg tolerance gives a 30 deg arc — wide enough that
     *  a casual "roughly that way" flick lands inside it, narrow enough that
     *  two adjacent cardinals (45 deg apart) never overlap. Hard-capped at 44
     *  deg internally so overlap is structurally impossible. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Assist",
        meta = (ClampMin = "1.2", ClampMax = "3.6"))
    float CaptureArcMultiplier = 2.5f;

    /** Peak assist rate at dead-centre of the arc (deg/sec). 60-100 reads as a
     *  gentle magnet; above ~140 players report the camera "fighting" them. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Assist",
        meta = (ClampMin = "0.0", ClampMax = "360.0"))
    float MaxAssistDegPerSec = 85.f;

    /** Player turn rate (deg/sec) at which assist begins to fade out. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Assist",
        meta = (ClampMin = "0.0"))
    float AssistFadeStartRate = 40.f;

    /** Player turn rate (deg/sec) at which assist is fully off. Above this the
     *  player is clearly sweeping the camera and any pull would read as lag. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Assist",
        meta = (ClampMin = "1.0"))
    float AssistFadeEndRate = 220.f;

    /** Inside this error the assist snaps exactly and stops writing. Prevents
     *  sub-degree float chatter from re-broadcasting the lock state. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Assist",
        meta = (ClampMin = "0.01", ClampMax = "2.0"))
    float SettleDeadZoneDegrees = 0.25f;

    // ═══════════════════════════════════════════════════════════════════════
    // LAYER 2 — Intent buffer tuning
    // ═══════════════════════════════════════════════════════════════════════

    /** Master switch for press latching. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Buffer")
    bool bIntentBufferEnabled = true;

    /** How long a latched press stays alive (seconds). Must be long enough to
     *  cover the worst-case slew: (CaptureArc / BufferedSlewDegPerSec). At the
     *  defaults that is 30/600 = 0.05s, so 0.40 is enormously generous while
     *  still being short enough that a mis-press never feels sticky. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Buffer",
        meta = (ClampMin = "0.05", ClampMax = "1.5"))
    float IntentBufferTime = 0.40f;

    /** Slew rate while an intent is latched (deg/sec). Fast and deliberate —
     *  this is a commanded move, not an assist, so it ignores the speed gate. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Buffer",
        meta = (ClampMin = "60.0", ClampMax = "1440.0"))
    float BufferedSlewDegPerSec = 600.f;

    /** If the player turns faster than this while an intent is latched, they
     *  have changed their mind — cancel silently rather than yanking them back. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Buffer",
        meta = (ClampMin = "30.0"))
    float IntentCancelYawRate = 300.f;

    // ═══════════════════════════════════════════════════════════════════════
    // Audio — routed through UkdAudioSubsystem::PlaySFX2D, never GameplayStatics
    // ═══════════════════════════════════════════════════════════════════════

    /** One-shot detent tick on Seeking -> Locked. A dry mechanical plate click.
     *  Keep it SHORT (<80ms) and quiet; it fires often. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Audio")
    TObjectPtr<USoundBase> RegisterLockSound = nullptr;

    /** Softer reverse tick on Locked -> Seeking/Idle. Optional; None is fine. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Audio")
    TObjectPtr<USoundBase> RegisterUnlockSound = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "kd|Crush|Audio",
        meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float RegisterTickVolume = 0.45f;

    // ═══════════════════════════════════════════════════════════════════════
    // Fallbacks / debug
    // ═══════════════════════════════════════════════════════════════════════

    /** Used only if the Ukd_CrushToggle CDO cannot be found at BeginPlay.
     *  A warning is logged if this path is ever taken. */
    UPROPERTY(EditDefaultsOnly, Category = "kd|Crush|Register",
        meta = (ClampMin = "1.0", ClampMax = "44.0"))
    float FallbackToleranceDegrees = 12.f;

    /** On-screen readout of error / register / state. Non-shipping only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|Crush|Debug")
    bool bDrawDebug = false;

private:
    // ── Per-frame pipeline (each step is one small, testable job) ────────────
    void  CacheOwners();
    bool  IsRegisterAvailable() const;
    void  EvaluateRegister(float CurrentYaw);
    float ComputeAssistStep(float DeltaTime, float SignedError) const;
    void  ApplyYaw(float NewYaw);
    void  SetRegisterState(EkdRegisterState NewState);
    void  ResetRegister();
    void  FireBufferedIntent();
    void  PlayTick(USoundBase* Sound) const;
    void  FireDenialCue() const;
    /** Non-const: sets bToleranceResolved on success. */
    float ResolveToleranceFromAbilityCDO();

    // ── Cached references (weak where the object can outlive/precede us) ─────
    UPROPERTY(Transient) TObjectPtr<AkdMyPlayer> CachedPlayer = nullptr;
    UPROPERTY(Transient) TObjectPtr<APlayerController> CachedPC = nullptr;
    UPROPERTY(Transient) TObjectPtr<UAbilitySystemComponent> CachedASC = nullptr;

    // ── Published state ─────────────────────────────────────────────────────
    float             Register01 = 0.f;
    float             AlignmentErrorDegrees = 90.f;
    float             ResolvedToleranceDegrees = 12.f;
    /** False until a Ukd_CrushToggle spec has actually been found on the ASC.
     *  Abilities are granted in AkdMyPlayer::BeginPlay, which runs AFTER every
     *  component's BeginPlay — so the CDO read must be retried, not assumed. */
    bool              bToleranceResolved = false;
    EkdRegisterState  RegisterState = EkdRegisterState::Idle;
    EkdCrushDirection PredictedDirection = EkdCrushDirection::None;

    // ── Assist bookkeeping ──────────────────────────────────────────────────
    /** The yaw this component last wrote. Comparing next frame's incoming yaw
     *  against THIS (not against last frame's raw yaw) isolates the player's
     *  own input from our assist contribution — otherwise the speed gate would
     *  measure its own output and oscillate. */
    float LastWrittenYaw = 0.f;
    bool  bHasWrittenYaw = false;
    float PlayerYawRateDegPerSec = 0.f;

    // ── Intent buffer bookkeeping ───────────────────────────────────────────
    bool  bIntentPending = false;
    float IntentTimeRemaining = 0.f;
    /** Re-entrancy guard: FireBufferedIntent -> RequestCrushToggle ->
     *  RequestCrushIntent must not latch a second buffer. */
    bool  bFiringBufferedIntent = false;
};
