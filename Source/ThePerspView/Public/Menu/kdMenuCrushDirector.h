// Copyright ASKD Games. ThePerspView.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdMenuCrushDirector.generated.h"

class UMaterialParameterCollection;
class UCurveFloat;
class APostProcessVolume;
class USpringArmComponent;

/**
 * UkdMenuCrushDirector
 *
 * Menu-only presentation driver that fakes in-game Crush Mode purely as a visual
 * effect on the title screen. It is DELIBERATELY decoupled from the gameplay
 * crush pipeline (GAS, UkdCharacterMovementComponent / PhysShadow2D,
 * UkdGeometryTransitionComponent, and the pawn-owned UkdWorldColorDriver):
 * the main menu is a diorama, not a simulation, so it must never instantiate or
 * depend on the pawn-driven, basis-aware crush systems. This avoids the
 * null-pawn-at-BeginPlay and single-writer hazards those systems carry.
 *
 * Responsibilities:
 *   - Interpolate a single 0..1 "crush alpha" (0 = Light / 3D, 1 = Crush / 2D).
 *   - Collapse each assigned target actor along a configurable, horizontal axis
 *     (toward the menu camera) by scaling its root component. Targets MUST be
 *     Movable mobility -- Static-mobility actors silently ignore runtime
 *     transforms (same gotcha as the gameplay collapse).
 *   - Act as the SOLE writer of MPC_WorldColor in the menu world, mirroring the
 *     single-writer rule UkdWorldColorDriver enforces in gameplay worlds. Any
 *     menu material/sky that already samples MPC_WorldColor will grade for free.
 *   - Optionally run an attract-mode oscillation before the first interaction,
 *     teaching the core verb directly on the wordmark.
 *
 * Drop this component on any actor in the menu level, assign CrushTargets and the
 * MPC, then drive it from the title-word buttons via UkdMenuPresentationSubsystem.
 */
UCLASS(ClassGroup = (Heliograph), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdMenuCrushDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UkdMenuCrushDirector();

	//~ Public API ----------------------------------------------------------------

	/** Drive the diorama toward Light (false) or Crush (true). bImmediate snaps with no transition. */
	UFUNCTION(BlueprintCallable, Category = "Heliograph|Menu")
	void SetCrushed(bool bCrushed, bool bImmediate = false);

	/** Flip between the committed Light and Crush states. */
	UFUNCTION(BlueprintCallable, Category = "Heliograph|Menu")
	void ToggleCrush();

	/** True once the last committed (clicked) state is the crushed state. */
	UFUNCTION(BlueprintPure, Category = "Heliograph|Menu")
	bool IsCrushed() const { return CommittedTarget >= 0.5f; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	//~ Config: collapse ----------------------------------------------------------

	/** Actors whose root component is scaled to fake the collapse. Each root MUST be Movable. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Targets")
	TArray<TObjectPtr<AActor>> CrushTargets;

	/**
	 * World-space axis the geometry flattens along. Default = world forward (X).
	 * Set this to the menu camera's forward vector so the diorama collapses toward
	 * the viewer. Keep it horizontal (Z = 0) so meshes stay seated on the platform.
	 */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Targets")
	FVector CollapseAxis = FVector(1.f, 0.f, 0.f);

	/** Residual thickness along the collapse axis when fully crushed (0.04 = 4%). */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Targets", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrushedThickness = 0.04f;

	//~ Config: transition --------------------------------------------------------

	/** Seconds for a Light<->Crush transition. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Transition", meta = (ClampMin = "0.01"))
	float TransitionDuration = 0.55f;

	/** Optional easing curve (0..1 -> 0..1). Falls back to smoothstep when null. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Transition")
	TObjectPtr<UCurveFloat> CrushCurve = nullptr;

	//~ Config: color (MPC) -------------------------------------------------------

	/** MPC_WorldColor. This component is its sole writer in the menu world. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Color")
	TObjectPtr<UMaterialParameterCollection> WorldColorMPC = nullptr;

	/** Scalar param names. Defaults match the canonical MPC_WorldColor (case-sensitive). */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Color")
	FName P_WorldBlendAlpha = TEXT("WorldBlendAlpha");

	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Color")
	FName P_ShadowTintAlpha = TEXT("ShadowTintAlpha");

	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Color")
	FName P_EdgePulseAlpha = TEXT("EdgePulseAlpha");

	/** Maximum shadow tint written at full crush. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Color", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxShadowTint = 1.0f;

	/** One-shot edge pulse length fired at the start of every transition. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Color", meta = (ClampMin = "0.0"))
	float EdgePulseDuration = 0.35f;

	//~ Config: post-process ------------------------------------------------------

	/**
	 * The two post-process volumes already placed in the menu level (same pair the
	 * gameplay levels use). The director crossfades their BlendWeight by crush alpha;
	 * it becomes the sole writer of BlendWeight on these two volumes in the menu.
	 *
	 * Editor setup: both volumes Unbound = true, the Shadow volume's Priority HIGHER
	 * than the Light volume's so it wins at full crush. Both already carry the
	 * M_CrushPostProcess blendable -- nothing about the materials is touched here.
	 */

	 /** Light (3D) look. Fully weighted at alpha 0. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|PostProcess")
	TObjectPtr<APostProcessVolume> LightVolume = nullptr;

	/** Crush / Shadow (2D) look. Fully weighted at alpha 1. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|PostProcess")
	TObjectPtr<APostProcessVolume> ShadowVolume = nullptr;

	/**
	 * true  = symmetric crossfade (Light = 1-alpha, Shadow = alpha).
	 * false = base+overlay (Light stays at 1, Shadow fades in over it). Flip to this
	 *         if the mid-transition looks muddy from both volumes fading at once.
	 */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|PostProcess")
	bool bCrossfadeVolumes = true;

	//~ Config: camera ------------------------------------------------------------

	/**
	 * The menu's display pawn is a copy that is NOT possessed, so it is located by
	 * actor tag rather than through the player controller. Add MenuPawnTag as an
	 * Actor Tag on the copy; the director grabs its first USpringArmComponent.
	 * Rotation is interpolated with FQuat::Slerp to stay pop-free across large
	 * angular deltas; differ pitch only between the two poses and leave yaw
	 * identical unless a yaw swing is intended.
	 */

	 /** Actor Tag on the unpossessed menu pawn copy. If None, falls back to a possessed player pawn. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Camera")
	FName MenuPawnTag = TEXT("MenuCameraRig");

	/** If true, LightArmRotation is captured from the arm's pose the first time it resolves, so you only author the Crush end. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Camera")
	bool bSeedLightRotationFromArm = true;

	/** Spring arm relative rotation at the Light state (alpha 0). Ignored while bSeedLightRotationFromArm is on. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Camera")
	FRotator LightArmRotation = FRotator::ZeroRotator;

	/** Spring arm relative rotation at the Crush state (alpha 1). */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Camera")
	FRotator CrushArmRotation = FRotator::ZeroRotator;

	//~ Config: attract mode ------------------------------------------------------

	/** Slowly oscillate Light<->Crush until the player first interacts (attract / "teach the verb"). */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Attract")
	bool bEnableAttractMode = true;

	/** Full Light->Crush->Light attract cycles per second. Kept intentionally slow. */
	UPROPERTY(EditAnywhere, Category = "Heliograph|Menu|Attract", meta = (ClampMin = "0.0"))
	float AttractCyclesPerSecond = 0.08f;

	//~ Runtime state -------------------------------------------------------------

	/** Per-target cached rest (uncrushed) relative scale; parallel array to CrushTargets. */
	UPROPERTY(Transient)
	TArray<FVector> RestScales;

	float CurrentAlpha = 0.f;     // Eased visual alpha actually applied this frame.
	float StartAlpha = 0.f;       // Alpha captured at the start of the active transition.
	float CommittedTarget = 0.f;  // 0 or 1 -- the last *committed* (clicked) state.
	float TransitionElapsed = 0.f;
	bool  bTransitioning = false;
	bool  bUserHasInteracted = false;
	float AttractPhase = 0.f;
	float PulseRemaining = 0.f;

	/** Boom on the spawned pawn, resolved lazily to survive the spawn/possession race. */
	UPROPERTY(Transient)
	TWeakObjectPtr<USpringArmComponent> CachedArm;

	/** Set once LightArmRotation has been captured from the resolved arm. */
	bool  bLightRotationSeeded = false;

	void  CacheRestState();
	void  ApplyAlphaToTargets(float Alpha) const;
	void  ApplyPostProcess(float Alpha) const;
	void  ApplySpringArm(float Alpha);
	USpringArmComponent* ResolveSpringArm();
	void  WriteColorParameters(float Alpha, float PulseAlpha) const;
	float EvaluateEase(float Linear01) const;
	void  BeginTransition(float NewTarget);
};