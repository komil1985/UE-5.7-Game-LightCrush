// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "kdRagdollComponent.generated.h"

class AkdMyPlayer;
class USkeletalMeshComponent;
class UCapsuleComponent;
class UkdPlayerHoverComponent;

// ─────────────────────────────────────────────────────────────────────────────
// UkdRagdollComponent — attached to AkdMyPlayer.
//
// Real Chaos simulation. The corpse is handed to the solver and left alone;
// this component owns the hand-off, not the motion. If a death looks wrong, the
// fix is in the Physics Asset, not here — no governors, no clamps, no scripted
// fallback. Those all existed at one point and every one of them was a symptom
// of a single-body physics asset that could not flop. See REQUIREMENTS.
//
// OWNERSHIP (single-writer rule):
//   While bRagdolling is true this component is the EXCLUSIVE owner of:
//     • USkeletalMeshComponent physics state, collision state, world transform
//     • UCapsuleComponent collision-enabled state
//     • UCharacterMovementComponent movement mode
//     • USpringArmComponent RELATIVE LOCATION (rotation + arm length stay with
//       UkdCrushTransitionComponent — the two never overlap)
//   It seizes that authority by suspending every other mesh-transform writer:
//     • UkdPlayerHoverComponent      → SetSuspended(true)  (explicit hand-off)
//     • UkdJumpSquashComponent       → self-suspends on State.Dead
//     • UkdCrushTransitionComponent  → aborted by UkdDeathComponent BEFORE entry
//
// NOT this component's job: applying State.Dead, cancelling abilities, or
// un-crushing the world. UkdDeathComponent orchestrates all of that.
//
// REQUIREMENTS — both are hard, and neither is fixable from C++:
//   1. The Skeletal Mesh needs a MULTI-BONE chain and a Physics Asset with
//      several bodies AND constraints between them. Bodies bind to bones and
//      constraints bind pairs of bodies, so a one-bone skeleton can only ever
//      produce one body and zero constraints — a marble that slides and rolls.
//      No amount of mass/damping/restitution tuning makes one body flop.
//   2. Every surface the corpse can land on needs SIMPLE collision. Rigid bodies
//      collide against simple primitives only; line traces also hit complex
//      collision, so a floor can report a clean trace hit and still be invisible
//      to physics. LogFloor() below reports this on every death.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdRagdollComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UkdRagdollComponent();

	/**
	 * Hands the mesh to Chaos. Idempotent — a second call while ragdolling is a
	 * no-op, so it is safe on every death path (light, fall, hazard).
	 *
	 * @param InheritedVelocity  The player's velocity the frame death was declared.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void EnterRagdoll(const FVector& InheritedVelocity);

	/**
	 * Returns the mesh to the capsule and restores every saved property.
	 * Idempotent and safe to call when not ragdolling.
	 *
	 * MUST run before any teleport: a simulating mesh ignores SetActorLocation,
	 * so teleporting first would move the capsule and leave the corpse behind.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void ExitRagdoll();

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	bool IsRagdolling() const { return bRagdolling; }

	// ── Config ────────────────────────────────────────────────────────────────
	// Five values. Deliberately. Every knob previously here was compensating for
	// a broken asset, and two of them (a tick gate and a stale BP override of a
	// speed clamp) silently disabled the very behaviour they were meant to
	// guarantee. Behaviour tuning belongs in the Physics Asset.

	/** Root of the physics chain. Drives the camera follow. Falls back to bone 0
	*  with a warning if the name isn't on the skeleton, so a rig re-import can
	*  never silently break the death camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll")
	FName PelvisBoneName = FName("root");

	/** Track the corpse with the camera. Moves the SPRING ARM, never the actor —
	*  see TickComponent for why that distinction is load-bearing. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll")
	bool bCameraFollowsRagdoll = true;

	/** Fraction of the death-frame velocity handed to the bodies. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityInheritance = 0.85f;

	/** Ceiling on the speed handed to the solver. A terminal fall arrives at
	*  ~1900 cm/s; CCD covers it, but there is no gameplay reason for a corpse to
	*  enter the world faster than this. Sanity bound, not a behaviour fix. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll", meta = (ClampMin = "0.0"))
	float MaxInheritedSpeed = 900.f;

	/** While the world is still collapsed, project body velocity onto the shadow
	*  plane so the corpse dies ON the plate instead of drifting off it. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll")
	bool bLockToCrushPlane = true;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<AkdMyPlayer> CachedPlayer;

	/** Resolved by class in BeginPlay, not through AkdMyPlayer::HoverComponent —
	*  a null native pointer would desync the suspend/resume pair silently. */
	UPROPERTY()
	TObjectPtr<UkdPlayerHoverComponent> CachedHover;

	bool bRagdolling = false;

	// ── Baselines — captured once in BeginPlay ────────────────────────────────
	FVector  BaselineMeshRelLoc = FVector::ZeroVector;
	FRotator BaselineMeshRelRot = FRotator::ZeroRotator;
	FVector  BaselineSpringArmRelLoc = FVector::ZeroVector;

	// ── Saved state — captured in EnterRagdoll, restored in ExitRagdoll ───────
	FVector                 SavedMeshRelScale = FVector::OneVector;
	FName                   SavedMeshProfile = NAME_None;
	ECollisionResponse      SavedMeshCameraResponse = ECR_Ignore;
	ECollisionEnabled::Type SavedCapsuleCollision = ECollisionEnabled::QueryAndPhysics;

	// ── Crush-plane lock ──────────────────────────────────────────────────────
	FVector LockedCollapseNormal = FVector::ZeroVector;
	bool    bPlaneLockActive = false;

	/** Tick does cosmetics ONLY (camera follow, plane lock). Nothing load-bearing
	*  lives here, so a config box switching it off can no longer break a death. */
	bool NeedsTick() const { return bRagdolling && (bCameraFollowsRagdoll || bPlaneLockActive); }

	FName ResolvePelvisBone(USkeletalMeshComponent* Mesh) const;

#if !UE_BUILD_SHIPPING
	/** Reports bodies/constraints/simulating — answers "is this actually a rig". */
	void LogRig(USkeletalMeshComponent* Mesh) const;

	/** Reports whether the surface below has SIMPLE collision — answers "can a
	*  rigid body land on this at all", which a line trace cannot tell you. */
	void LogFloor(USkeletalMeshComponent* Mesh) const;
#endif
};