// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdJumpSquashComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJumpLaunched, float, LaunchSpeed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJumpApex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJumpLanded, float, ImpactSpeed, bool, bHardLand);

class USkeletalMeshComponent;
class UCharacterMovementComponent;
class UAbilitySystemComponent;
class UNiagaraComponent;
struct FHitResult;

// ─────────────────────────────────────────────────────────────────────────────
// UkdJumpSquashComponent
//
// Cosmetic-only procedural squash & stretch for the player's body mesh
// (GetMesh()), driven entirely by CharacterMovementComponent's actual
// physics state — NOT by input events. It therefore reacts correctly
// whether airborne state was caused by a normal Jump(), an ability launch
// (e.g. UkdShadowMove::LaunchCharacter), a future bounce pad, or simply
// walking off a ledge — zero extra wiring required anywhere else.
//
// SINGLE-WRITER NOTE:
//   This component is the sole writer of BodyMesh's RelativeScale3D while
//   the player is OUTSIDE Crush Mode. The instant State.CrushMode goes
//   active, it zeroes its internal offsets and stops writing entirely,
//   handing scale-write authority to UkdCrushTransitionComponent — same
//   rule as the MPC_WorldColor single-writer convention, applied to mesh
//   transform instead of a material parameter collection.
//   UkdPlayerHoverComponent remains the sole writer of BodyMesh's
//   RelativeLocation and of the eye meshes' RelativeScale3D; this
//   component never touches either, so the two can never fight. Because
//   EyeLeft/EyeRight are attached as children of GetMesh(), they inherit
//   this component's body scale automatically through the scene
//   hierarchy — no extra code needed for the eyes to squash/stretch too.
//
// PHASES (derived from Velocity.Z, not time):
//   1. LAUNCH    — fires the instant MovementMode flips grounded -> Falling
//                  with upward velocity. Plays a fast compress -> stretch
//                  -> settle pop (three-phase curve, same smoothstep-lerp
//                  family already used by UkdCrushTransitionComponent's
//                  exit-transition dip/pop shape).
//   2. AIRBORNE  — every tick while falling, blends a stretch/neutral/squash
//                  offset purely from current Velocity.Z, so it continuously
//                  tracks the real arc (rising fast = stretched, near apex
//                  = neutral, falling fast = squashed in anticipation of
//                  landing). Works correctly even with no LAUNCH phase
//                  (e.g. ran off a ledge — no apex, just a fall).
//   3. APEX      — fired by AkdMyPlayer::NotifyJumpApex() (one-line virtual
//                  override — ACharacter has no existing apex delegate to
//                  bind to, unlike Landed). Optional secondary "hang" beat
//                  layered on top of the velocity-driven airborne pose.
//                  NOTE: this only fires for arcs the engine armed via a
//                  real upward launch; pure ledge-walk-offs correctly never
//                  fire it (there is no apex to report).
//   4. LANDING   — bound to ACharacter::LandedDelegate, the SAME multicast
//                  delegate UkdFallDamageComponent already listens to.
//                  Multiple listeners are fine; this isn't a single-writer
//                  field. Impact squash scaled by landing speed, then a
//                  damped spring overshoot back to neutral (same shape
//                  family as UkdCrushTransitionComponent::SettleOvershootAmount).
//
// All three OnJump* delegates are BlueprintAssignable so UkdGameFeedbackComponent
// (or anything else) can bind camera shake / Niagara / audio without this
// component knowing anything about cameras, particles or sound — same
// separation of concerns as every other component in this project.
//
// SETUP:
//   1. AkdMyPlayer constructor:
//        JumpSquashComponent = CreateDefaultSubobject<UkdJumpSquashComponent>(TEXT("JumpSquashComponent"));
//   2. AkdMyPlayer.h: forward-declare + UPROPERTY (see chat for full diff).
//   3. Override AkdMyPlayer::NotifyJumpApex() to call
//        JumpSquashComponent->NotifyApex();
//   4. No other wiring required — LandedDelegate binding and movement-mode
//      polling happen internally in BeginPlay / TickComponent.
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(ClassGroup = ("ASKD"), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdJumpSquashComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UkdJumpSquashComponent();

	/** Call from AkdMyPlayer::NotifyJumpApex(). See class comment. */
	UFUNCTION(BlueprintCallable, Category = "Jump Squash")
	void NotifyApex();

	// ── Delegates — bind from UkdGameFeedbackComponent for shake/SFX/VFX ──────
	UPROPERTY(BlueprintAssignable, Category = "Jump Squash")
	FOnJumpLaunched OnJumpLaunched;

	UPROPERTY(BlueprintAssignable, Category = "Jump Squash")
	FOnJumpApex OnJumpApex;

	UPROPERTY(BlueprintAssignable, Category = "Jump Squash")
	FOnJumpLanded OnJumpLanded;

	// ── Launch pop ──────────────────────────────────────────────────────────

	/** Seconds for the full compress -> stretch -> settle launch pop. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Launch", meta = (ClampMin = "0.05", ClampMax = "0.6"))
	float LaunchPopDuration = 0.22f;

	/** Normalized time [0-1] at which the compress phase ends and the spring into stretch begins. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Launch", meta = (ClampMin = "0.05", ClampMax = "0.45"))
	float LaunchCompressFraction = 0.18f;

	/** Normalized time [0-1] at which the stretch reaches its peak before settling to neutral. Must be > LaunchCompressFraction. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Launch", meta = (ClampMin = "0.2", ClampMax = "0.9"))
	float LaunchStretchPeakFraction = 0.55f;

	/** How short (Z) / wide (XY) the body compresses at the very start of the pop. 0.22 = 22% squash. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Launch", meta = (ClampMin = "0.0", ClampMax = "0.6"))
	float LaunchCompressAmount = 0.22f;

	/** How tall (Z) / thin (XY) the body stretches at peak pop. 0.28 = 28% stretch. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Launch", meta = (ClampMin = "0.0", ClampMax = "0.6"))
	float LaunchStretchAmount = 0.28f;

	/** Minimum upward speed (cm/s) at liftoff required to count as a "launch" rather than residual ground jitter. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Launch", meta = (ClampMin = "0.0"))
	float MinLaunchSpeed = 80.f;

	// ── Airborne (continuous, velocity-driven) ────────────────────────────────

	/** Vertical speed (cm/s) at which the rising stretch reaches full strength. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Airborne", meta = (ClampMin = "10.0"))
	float RiseSpeedForFullStretch = 700.f;

	/** Vertical speed (cm/s) at which the falling squash (landing anticipation) reaches full strength. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Airborne", meta = (ClampMin = "10.0"))
	float FallSpeedForFullSquash = 900.f;

	/** Max Z stretch fraction while rising. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Airborne", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MaxRiseStretch = 0.16f;

	/** Max Z squash fraction while falling fast. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Airborne", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MaxFallSquash = 0.14f;

	/** How quickly the airborne pose chases its velocity-derived target (per second). Higher = snappier. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Airborne", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float AirborneBlendSpeed = 10.f;

	// ── Apex ────────────────────────────────────────────────────────────────

	/** Extra uniform "puff" scale added briefly at the jump apex. 0 disables the apex layer entirely. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Apex", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float ApexPuffAmount = 0.06f;

	/** Seconds for the apex puff to rise and fall (single sine hump). */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Apex", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ApexPuffDuration = 0.30f;

	// ── Landing impact ─────────────────────────────────────────────────────────

	/** Impact speed (cm/s) below which landings are ignored entirely (e.g. stepping off a 2cm ledge). */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Landing", meta = (ClampMin = "0.0"))
	float MinLandingSpeed = 150.f;

	/** Impact speed (cm/s) that produces the FULL landing squash; speeds above this clamp. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Landing", meta = (ClampMin = "50.0"))
	float MaxLandingSpeedForFullSquash = 1200.f;

	/**
	 * Impact speed (cm/s) above which OnJumpLanded reports bHardLand = true.
	 * Deliberately a separate, self-contained threshold rather than reading
	 * UkdFallDamageComponent::HardLandSpeed directly — visual polish should
	 * not runtime-depend on the gameplay-damage component. Keep the two
	 * numbers equal by convention if you want "hard land" to mean the same
	 * thing visually and mechanically (defaults already match: 800).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Landing", meta = (ClampMin = "50.0"))
	float HardLandThreshold = 800.f;

	/** Max squash fraction at full-strength landing impact. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Landing", meta = (ClampMin = "0.0", ClampMax = "0.7"))
	float LandingSquashAmount = 0.34f;

	/** Fraction past neutral to overshoot (stretch) as the squash springs back, scaled by impact strength. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Landing", meta = (ClampMin = "0.0", ClampMax = "0.4"))
	float LandingOvershootAmount = 0.10f;

	/** Base seconds for the squash -> overshoot -> settle sequence. Scales slightly with impact speed at runtime — harder landings settle a bit slower, reads as "heavier". */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Landing", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LandingSettleDuration = 0.32f;

	/**
	* Wire up the tentacle Niagara components from AkdMyPlayer::BeginPlay.
	* Mirrors UkdPlayerHoverComponent::SetMeshComponents — safe to call
	* multiple times, safe to pass a partial or empty array.
	*/
	UFUNCTION(BlueprintCallable, Category = "Jump Squash | Tentacles")
	void SetTentacleComponents(const TArray<UNiagaraComponent*>& InTentacles);

	// ── Tentacle reactivity ──────────────────────────────────────────────────
	//
	// Drives Niagara User Parameters on the tentacle components using the SAME
	// curves already computed for the body squash above — zero extra curve
	// authoring. Each value is a 0-1 alpha; what the Niagara system DOES with
	// it (a force impulse, a spawn burst, a stretch) is entirely up to the
	// graph. Calling SetVariableFloat with a name that doesn't exist on a
	// given Niagara System is a safe no-op, so it's fine if not every tentacle
	// system has all three wired yet.

	/** Master toggle — false if your Niagara systems aren't wired with these params yet. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Tentacles")
	bool bDriveTentacleParameters = true;

	/** 0-1, peaks during the launch pop (compress through stretch). Suggested use: a brief backward/downward force impulse — reads as the tentacles thrusting the launch. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Tentacles")
	FName TentacleLaunchParamName = FName("LaunchKick");

	/** 0-1, rises with fall speed (same curve as the body's falling squash). Suggested use: drag/wind force strength or trail stretch, so tentacles visibly stream as you fall faster. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Tentacles")
	FName TentacleFallDragParamName = FName("FallDrag");

	/** 0-1, follows the landing squash/overshoot curve. Suggested use: a radial splay displacement or a one-shot spawn burst on impact. */
	UPROPERTY(EditDefaultsOnly, Category = "Jump Squash | Tentacles")
	FName TentacleLandImpactParamName = FName("LandImpact");

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void OnCharacterLanded(const FHitResult& Hit);

	void TickAirborne(float DeltaTime);
	void TickLaunchPop(float DeltaTime);
	void TickApexPuff(float DeltaTime);
	void TickLanding(float DeltaTime);
	void ApplyFinalScale();

	bool IsSuspendedForCrushMode() const;

	// ── Cached refs ─────────────────────────────────────────────────────────
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> BodyMesh;
	UPROPERTY() TObjectPtr<class ACharacter> CachedOwner;
	UPROPERTY() TObjectPtr<UCharacterMovementComponent> CachedMoveComp;
	UPROPERTY() TObjectPtr<UAbilitySystemComponent> CachedASC; // may be null

	FVector BodyBaseScale = FVector::OneVector;
	bool bWasFalling = false;

	// Independent additive layers, composed together each tick in ApplyFinalScale().
	FVector AirborneScaleOffset = FVector::ZeroVector; // continuous, velocity-driven
	FVector LaunchPopOffset = FVector::ZeroVector; // one-shot, on liftoff
	FVector ApexPuffOffset = FVector::ZeroVector; // one-shot, at apex
	FVector LandingOffset = FVector::ZeroVector; // one-shot, on impact

	bool  bLaunchPopActive = false;
	float LaunchPopElapsed = 0.f;

	bool  bApexPuffActive = false;
	float ApexPuffElapsed = 0.f;

	bool  bLandingActive = false;
	float LandingElapsed = 0.f;
	float LandingDurationActual = 0.32f;
	float LandingPeakSquash = 0.f;

	void UpdateTentacleParameters();
	void ZeroTentacleParameters();

	UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> Tentacles;
};