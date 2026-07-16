// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "kdRagdollComponent.generated.h"

class AkdMyPlayer;
class USkeletalMeshComponent;
class UCapsuleComponent;

// ─────────────────────────────────────────────────────────────────────────────
// UkdRagdollComponent — attached to AkdMyPlayer.
//
// OWNERSHIP (single-writer rule):
//   While bRagdolling is true this component is the EXCLUSIVE owner of:
//     • USkeletalMeshComponent physics state + world transform
//     • UCapsuleComponent collision-enabled state
//     • UCharacterMovementComponent movement mode
//   It seizes that authority by suspending every other mesh-transform writer:
//     • UkdPlayerHoverComponent  → SetSuspended(true)   (explicit hand-off)
//     • UkdJumpSquashComponent   → self-suspends on State.Dead
//     • UkdCrushTransitionComponent → aborted by UkdDeathComponent BEFORE entry
//
// NOT this component's job: applying State.Dead, cancelling abilities, or
// un-crushing the world. UkdDeathComponent orchestrates all of that.
//
// REQUIREMENTS:
//   The player's Skeletal Mesh MUST have a Physics Asset. Without one this
//   component logs an error and no-ops — the rest of the death sequence still
//   runs, so a missing asset degrades, it never soft-locks.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdRagdollComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UkdRagdollComponent();

	/**
	 * Hands the mesh to Chaos. Idempotent — a second call while ragdolling is a
	 * no-op, so it is safe on every death path (light, fall, enemy).
	 *
	 * @param InheritedVelocity  The player's velocity the frame death was declared.
	 *                           Carried into the corpse so the death reads as
	 *                           continuous motion, not a spawned prop.
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

	/** Collision profile applied to the mesh while simulating. Engine default
	*  "Ragdoll" = PhysicsBody, blocks WorldStatic/WorldDynamic, ignores Pawn. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Collision")
	TEnumAsByte<ECollisionChannel> RagdollObjectChannel = ECC_Pawn;

	/** Root/pelvis bone. Drives camera follow. Auto-falls back to bone 0 if the
	*  name isn't on the skeleton, so a rename can't silently break the camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Camera")
	FName PelvisBoneName = FName("pelvis");

	/** Pin the capsule (and therefore the SpringArm) to the pelvis each frame so
	*  the camera stays on the body if it rolls or slides off a ledge. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Camera")
	bool bCameraFollowsRagdoll = true;

	/** Fraction of the player's death-frame velocity handed to the bodies. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Impulse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityInheritance = 0.85f;

	/** Upward kick at death, in cm/s. Applied as a VELOCITY CHANGE, not a force,
	*  so it is mass-independent — critical because a mesh at Crush scale (0.2)
	*  has ~1/125th the mass of the 3D body and would otherwise launch. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Impulse", meta = (ClampMin = "0.0"))
	float DeathPopSpeed = 180.f;

	/** While the world is still collapsed, damp the corpse's velocity along the
	*  crush collapse axis so it dies ON the shadow plane instead of drifting
	*  off it. Pure polish, zero cost outside the death window. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Crush")
	bool bLockToCrushPlane = true;

	/**
	 * Dead-drop. The corpse is released with ZERO momentum and heavy damping —
	 * it falls where it stood and stays there.
	 *
	 * This is the shipping default. At 0.2 mesh scale the body has ~1/125th the
	 * authored mass, so handing it real velocity makes every death a physics
	 * lottery. Only turn this off if you want kinetic deaths AND have tuned mass,
	 * damping and restitution in the Physics Asset first.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Motion")
	bool bDeadDrop = true;

	/** Hard ceiling on corpse speed, enforced every frame while bDeadDrop.
	*  A governor, not a tuning value: depenetration launches, bounces and
	*  contact impulses all become physically incapable of exceeding this. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Motion", meta = (ClampMin = "0.0"))
	float MaxCorpseSpeed = 400.f;

	/** Zero angular velocity every frame while bDeadDrop. A blob has no readable
	*  orientation, so tumbling adds nothing and is where every "spins like a top"
	*  report comes from. Cheaper and more deterministic than damping. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Motion")
	bool bSuppressCorpseSpin = true;

	/** Lift the mesh clear of the floor before simulation starts.
	*  Your body reach (56.3) exceeds the clearance the capsule gives the mesh
	*  origin (37.1), so a grounded death is born 19.2cm inside the floor and
	*  Chaos ejects it. Lifting is the only fix that removes the impulse rather
	*  than capping it after the fact. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Placement")
	bool bLiftOutOfPenetration = true;

	/** Extra gap left above the floor when lifting. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Placement", meta = (ClampMin = "0.0"))
	float SpawnClearance = 2.f;

	/** Seconds for the collapse. Short — this plays under a 1.2s fade. */
	UPROPERTY(EditDefaultsOnly, Category = "Death Drop", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float DropDuration = 0.45f;

	/** Squash at the moment of impact. 0.4 = 40% flatter, 40% wider. */
	UPROPERTY(EditDefaultsOnly, Category = "Death Drop", meta = (ClampMin = "0.0", ClampMax = "0.8"))
	float ImpactSquash = 0.4f;

	/** Seconds for the squash to settle back out. */
	UPROPERTY(EditDefaultsOnly, Category = "Death Drop", meta = (ClampMin = "0.0", ClampMax = "0.8"))
	float SettleDuration = 0.25f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	bool NeedsTick() const
	{
		// bDeadDrop MUST be in here. The governor lives in TickComponent, so
		// gating the tick on camera-follow alone let a cosmetic checkbox in the
		// Details panel silently disable the constraint that guarantees the
		// corpse behaves. Config must never be able to switch off a safety system.
		return bRagdolling && (bDeadDrop || bCameraFollowsRagdoll || bPlaneLockActive);
	}

private:
	UPROPERTY()
	TObjectPtr<AkdMyPlayer> CachedPlayer;

	bool bRagdolling = false;

	// ── Saved state (restored verbatim in ExitRagdoll) ────────────────────────
	FVector                BaselineMeshRelLoc = FVector::ZeroVector;
	FRotator               BaselineMeshRelRot = FRotator::ZeroRotator;
	FVector                SavedMeshRelScale = FVector::OneVector;
	FName                  SavedMeshProfile = NAME_None;
	ECollisionResponse     SavedMeshCameraResponse = ECR_Ignore;
	ECollisionEnabled::Type SavedCapsuleCollision = ECollisionEnabled::QueryAndPhysics;
	float                  CachedCapsuleHalfHeight = 88.f;
	float   DropElapsed = 0.f;
	FVector DropStartLoc = FVector::ZeroVector;
	FVector DropEndLoc = FVector::ZeroVector;
	bool    bLanded = false;
	float   SettleElapsed = 0.f;

	FVector BaselineSpringArmRelLoc = FVector::ZeroVector;

	/** Hard cap on the speed handed to Chaos. A terminal fall arrives at ~1900 cm/s
	*  — at 60fps that is 26cm of travel per substep, further than this body's own
	*  radius at 0.2 scale. It tunnels into the floor and gets rocketed out by
	*  depenetration. Clamp before the solver ever sees it. */
	UPROPERTY(EditDefaultsOnly, Category = "Ragdoll | Impulse", meta = (ClampMin = "0.0"))
	float MaxInheritedSpeed = 700.f;

	// ── Crush-plane lock ──────────────────────────────────────────────────────
	FVector LockedCollapseNormal = FVector::ZeroVector;
	bool    bPlaneLockActive = false;

	bool bGovernorLogged = false;
	//bool NeedsTick() const { return bRagdolling && (bCameraFollowsRagdoll || bPlaneLockActive); }
	FName ResolvePelvisBone(USkeletalMeshComponent* Mesh) const;

#if !UE_BUILD_SHIPPING
	/** Where the body will be born relative to the floor beneath it. */
	struct FkdRagdollPlacement
	{
		bool    bFoundFloor = false;
		float   Penetration = 0.f;   // >0 = body starts this deep INSIDE the floor
		FString FloorName;
	};

	FkdRagdollPlacement ProbePlacement(USkeletalMeshComponent* Mesh) const;
#endif
};
