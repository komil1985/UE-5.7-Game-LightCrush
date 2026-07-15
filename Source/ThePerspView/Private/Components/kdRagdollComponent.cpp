// Copyright ASKD Games

#include "Components/kdRagdollComponent.h"
#include "Components/kdPlayerHoverComponent.h"
#include "Player/kdMyPlayer.h"
#include "Crush/kdCrushDirectionLibrary.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/SpringArmComponent.h"



UkdRagdollComponent::UkdRagdollComponent()
{
	// Deferred-tick pattern: zero idle cost, enabled only while a corpse exists.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UkdRagdollComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedPlayer = Cast<AkdMyPlayer>(GetOwner());
	if (!CachedPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[kdRagdoll] Owner is not AkdMyPlayer — component disabled."));
		return;
	}

	// Capture the canonical mesh anchor BEFORE any tick runs. Component BeginPlay
	// is guaranteed to complete for all components before the first TickComponent,
	// so this is the designer-authored pose, not a hover-offset frame.
	if (const USkeletalMeshComponent* Mesh = CachedPlayer->GetMesh())
	{
		BaselineMeshRelLoc = Mesh->GetRelativeLocation();
		BaselineMeshRelRot = Mesh->GetRelativeRotation();
	}

	if (const USpringArmComponent* Arm = CachedPlayer->SpringArm)
	{
		BaselineSpringArmRelLoc = Arm->GetRelativeLocation();
	}

	if (const UCapsuleComponent* Capsule = CachedPlayer->GetCapsuleComponent())
	{
		CachedCapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	}
}

FName UkdRagdollComponent::ResolvePelvisBone(USkeletalMeshComponent* Mesh) const
{
	if (!Mesh) return NAME_None;

	if (Mesh->GetBoneIndex(PelvisBoneName) != INDEX_NONE)
	{
		return PelvisBoneName;
	}

	// A skeleton rename must never silently break the death camera.
	const FName Fallback = (Mesh->GetNumBones() > 0) ? Mesh->GetBoneName(0) : NAME_None;
	UE_LOG(LogTemp, Warning,
		TEXT("[kdRagdoll] PelvisBoneName '%s' not on skeleton — falling back to root bone '%s'."),
		*PelvisBoneName.ToString(), *Fallback.ToString());
	return Fallback;
}

// ─────────────────────────────────────────────────────────────────────────────
// EnterRagdoll
// ─────────────────────────────────────────────────────────────────────────────
void UkdRagdollComponent::EnterRagdoll(const FVector& InheritedVelocity)
{
	if (bRagdolling || !IsValid(CachedPlayer)) return;

	USkeletalMeshComponent* Mesh = CachedPlayer->GetMesh();
	if (!IsValid(Mesh)) return;

	// ── 0. Gate: no Physics Asset = no bodies = SetSimulatePhysics is a silent
	//        no-op that leaves the mesh frozen mid-air. Fail loud, degrade safe.
	if (!Mesh->GetPhysicsAsset())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[kdRagdoll] Skeletal Mesh '%s' has NO Physics Asset — ragdoll skipped. "
				"Right-click the Skeletal Mesh asset → Create → Physics Asset. The rest "
				"of the death sequence will still run."),
			*GetNameSafe(Mesh->GetSkeletalMeshAsset()));
		return;
	}

	// ── 1. Scale sanity. Chaos bakes body scale at simulation start and does not
	//        handle non-uniform scale on convex/sphyl bodies — they interpenetrate
	//        and blow up. UkdCrushTransitionComponent's Z squash-stretch produces
	//        exactly that mid-morph, so a death timed into a morph would hit it.
	//        UkdDeathComponent aborts the transition before calling us, but this
	//        stays as the last line of defence.
	FVector Scale = Mesh->GetRelativeScale3D();
	if (!Scale.AllComponentsEqual(0.001f))
	{
		const float Uniform = (Scale.X + Scale.Y + Scale.Z) / 3.f;
		UE_LOG(LogTemp, Warning,
			TEXT("[kdRagdoll] Mesh scale %s is non-uniform — forcing uniform %.3f before sim."),
			*Scale.ToString(), Uniform);
		Scale = FVector(Uniform);
		Mesh->SetRelativeScale3D(Scale);
	}

	// ── 2. Seize mesh-transform authority from the cosmetic drivers ───────────
	//        UkdJumpSquashComponent self-suspends on State.Dead (already applied
	//        by UkdDeathComponent before this call). UkdPlayerHoverComponent has
	//        no ASC by design — it is told explicitly.
	if (UkdPlayerHoverComponent* Hover = CachedPlayer->HoverComponent)
	{
		Hover->SetSuspended(true);
	}

	// ── 3. Hand off the capsule ───────────────────────────────────────────────
	if (UCharacterMovementComponent* MC = CachedPlayer->GetCharacterMovement())
	{
		MC->StopMovementImmediately();
		MC->DisableMovement();   // MOVE_None — the CMC stops writing the capsule
	}

	if (UCapsuleComponent* Capsule = CachedPlayer->GetCapsuleComponent())
	{
		CachedCapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		SavedCapsuleCollision = Capsule->GetCollisionEnabled();
		// Off, or the capsule ploughs the corpse around and blocks its own fall.
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// ── 4. Mesh → physics ─────────────────────────────────────────────────────
	SavedMeshRelScale = Scale;
	SavedMeshProfile = Mesh->GetCollisionProfileName();
	SavedMeshCameraResponse = Mesh->GetCollisionResponseToChannel(ECC_Camera);

	// Built explicitly, NOT from the "Ragdoll" preset. The preset would set
	// ObjectType=PhysicsBody; the floors only block Pawn, so the corpse would
	// have no floor to land on. See RagdollObjectChannel's comment.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(RagdollObjectChannel);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);  // SpringArm probe
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);  // shadow traces

#if !UE_BUILD_SHIPPING
	LogPlacementProbe(Mesh);   // BEFORE sim starts — captures the spawn condition
#endif

	Mesh->SetSimulatePhysics(true);
	Mesh->SetAllBodiesPhysicsBlendWeight(1.f);

	// ── 5. Motion ─────────────────────────────────────────────────────────────
	if (bDeadDrop)
	{
		// Nothing but gravity. No inherited velocity, no pop, no spin — the two
		// things that were turning a 0.008x-volume body into a projectile.
		Mesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	}
	else
	{
		FVector StartVel = InheritedVelocity * VelocityInheritance;
		if (MaxInheritedSpeed > KINDA_SMALL_NUMBER)
		{
			StartVel = StartVel.GetClampedToMaxSize(MaxInheritedSpeed);
		}
		Mesh->SetAllPhysicsLinearVelocity(StartVel);
		Mesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);

		// CCD only matters when the body is actually moving fast. In dead-drop it
		// costs solver time for nothing, and CCD on an already-penetrating body
		// can sweep it somewhere arbitrary — which is the "flies through the map".
		Mesh->SetAllUseCCD(true);

		if (DeathPopSpeed > KINDA_SMALL_NUMBER)
		{
			Mesh->AddImpulse(FVector(0.f, 0.f, DeathPopSpeed),
				ResolvePelvisBone(Mesh), /*bVelChange=*/true);
		}
	}

	Mesh->WakeAllRigidBodies();

	// ── 6. Crush-plane lock ───────────────────────────────────────────────────
	bPlaneLockActive = false;
	if (bLockToCrushPlane)
	{
		const UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
		if (ASC && ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
		{
			LockedCollapseNormal = UkdCrushDirectionLibrary::MakeCrushBasis(
				CachedPlayer->GetActiveCrushDirection()).CollapseNormal;
			bPlaneLockActive = !LockedCollapseNormal.IsNearlyZero();
		}
	}

	bRagdolling = true;
	SetComponentTickEnabled(NeedsTick());
}

// ─────────────────────────────────────────────────────────────────────────────
// ExitRagdoll
// ─────────────────────────────────────────────────────────────────────────────
void UkdRagdollComponent::ExitRagdoll()
{
	if (!bRagdolling) return;

	bRagdolling = false;
	bPlaneLockActive = false;
	SetComponentTickEnabled(false);

	if (!IsValid(CachedPlayer)) return;

	if (USkeletalMeshComponent* Mesh = CachedPlayer->GetMesh())
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetAllBodiesPhysicsBlendWeight(0.f);   // hand the pose back to anim
		Mesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		Mesh->PutAllRigidBodiesToSleep();

		// Physics drove the COMPONENT transform, not just the bones — the mesh's
		// relative transform to the capsule is now garbage. Nothing else in the
		// codebase re-derives it, so restoring the BeginPlay baseline here is
		// mandatory or the mesh is permanently offset after the first death.
		if (UCapsuleComponent* Capsule = CachedPlayer->GetCapsuleComponent())
		{
			if (Mesh->GetAttachParent() != Capsule)
			{
				Mesh->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
		Mesh->SetRelativeLocationAndRotation(BaselineMeshRelLoc, BaselineMeshRelRot);
		Mesh->SetRelativeScale3D(SavedMeshRelScale);

		Mesh->SetCollisionProfileName(SavedMeshProfile);
		Mesh->SetCollisionResponseToChannel(ECC_Camera, SavedMeshCameraResponse);
	}

	if (USpringArmComponent* Arm = CachedPlayer->SpringArm)
	{
		Arm->SetRelativeLocation(BaselineSpringArmRelLoc);
	}

	if (UCapsuleComponent* Capsule = CachedPlayer->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(SavedCapsuleCollision);
	}

	// MOVE_Falling, not MOVE_Walking — matches UkdCrushStateComponent::ResetPhysicsTo3D.
	// Gravity resolves the landing rather than us asserting a floor we haven't traced.
	if (UCharacterMovementComponent* MC = CachedPlayer->GetCharacterMovement())
	{
		MC->SetMovementMode(MOVE_Falling);
	}

	// Release the cosmetic drivers. JumpSquash resumes on its own when State.Dead
	// is cleared in UkdDeathComponent::ClearAllGASDeathState.
	if (UkdPlayerHoverComponent* Hover = CachedPlayer->HoverComponent)
	{
		Hover->SetSuspended(false);
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[kdRagdoll] EXIT — mesh re-anchored to capsule."));
#endif
}

#if !UE_BUILD_SHIPPING
void UkdRagdollComponent::LogPlacementProbe(USkeletalMeshComponent* Mesh) const
{
	if (!Mesh || !GetWorld() || !CachedPlayer) return;

	const FVector Origin = Mesh->GetComponentLocation();
	const float   Radius = Mesh->Bounds.SphereRadius;

	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(kdRagdollProbe), /*bTraceComplex=*/false, CachedPlayer);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, Origin, Origin - FVector(0.f, 0.f, 5000.f), RagdollObjectChannel, P);

	if (!bHit)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[kdRagdoll] PROBE | NOTHING below within 50m on the corpse's channel — it will fall forever."));
		return;
	}

	const float Gap = (Origin.Z - Radius) - Hit.ImpactPoint.Z;

	UE_LOG(LogTemp, Log,
		TEXT("[kdRagdoll] PROBE | floor='%s'  bodyRadius=%.1f  gap=%.1fcm%s"),
		*GetNameSafe(Hit.GetActor()), Radius, Gap,
		Gap < 0.f ? TEXT("   <-- NEGATIVE: body spawns INSIDE the floor, depenetration will launch it")
		: TEXT(""));

	if (const UPrimitiveComponent* Floor = Hit.GetComponent())
	{
		const ECollisionResponse R = Floor->GetCollisionResponseToChannel(RagdollObjectChannel);
		UE_LOG(LogTemp, Log,
			TEXT("[kdRagdoll] PROBE | '%s' (profile '%s') responds to corpse channel with: %s"),
			*Floor->GetName(), *Floor->GetCollisionProfileName().ToString(),
			R == ECR_Block ? TEXT("BLOCK  (good)")
			: R == ECR_Overlap ? TEXT("OVERLAP  <-- corpse falls through")
			: TEXT("IGNORE  <-- corpse falls through"));
	}
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// TickComponent — only alive between EnterRagdoll and ExitRagdoll.
// ─────────────────────────────────────────────────────────────────────────────
void UkdRagdollComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bRagdolling || !IsValid(CachedPlayer))
	{
		SetComponentTickEnabled(false);
		return;
	}

	USkeletalMeshComponent* Mesh = CachedPlayer->GetMesh();
	if (!IsValid(Mesh)) { SetComponentTickEnabled(false); return; }

	// ── Hold the corpse on the shadow plane ───────────────────────────────────
	// Velocity projection only — snapping body transforms every frame would
	// fight the solver. Over a 1.2s fade this reads as a perfect plane lock.
	if (bPlaneLockActive)
	{
		for (FBodyInstance* Body : Mesh->Bodies)
		{
			// NOTE: deliberately NOT FBodyInstance::IsInstanceSimulatingPhysics().
			// That inline forwards to FBodyInstanceCore::ShouldInstanceSimulatingPhysics(),
			// which is PHYSICSCORE_API — visible to the compiler, not linked to this
			// module. bSimulatePhysics is a plain UPROPERTY bitfield on the same
			// struct, so reading it is header-only and needs no import. Every body
			// here was set simulating in EnterRagdoll, and SetLinearVelocity safely
			// no-ops on an invalid handle, so this is the complete guard.
			if (!Body || !Body->bSimulatePhysics) continue;

			const FVector V = Body->GetUnrealWorldVelocity();
			Body->SetLinearVelocity(FVector::VectorPlaneProject(V, LockedCollapseNormal), false);
		}
	}

	if (bCameraFollowsRagdoll)
	{
		// Move the SPRING ARM. Never the actor.
		//
		// The previous version called SetActorLocation(..., ETeleportType::TeleportPhysics).
		// TeleportPhysics is precisely the flag that makes UpdateKinematicBonesToAnim
		// force-move bodies EVEN WHEN THEY ARE SIMULATING. So every frame we teleported
		// the corpse onto the capsule, the solver differenced that jump against its own
		// integration and turned the delta into velocity — and it fed back on itself:
		// capsule chases pelvis → teleport moves pelvis → capsule chases again.
		//
		// The SpringArm is not a physics body, and UkdCrushTransitionComponent writes
		// only its rotation and TargetArmLength — never its location. Clean hand-off,
		// zero physics contact.
		if (USpringArmComponent* Arm = CachedPlayer->SpringArm)
		{
			const FName Bone = ResolvePelvisBone(Mesh);
			if (Bone != NAME_None)
			{
				const FVector Pelvis = Mesh->GetBoneLocation(Bone, EBoneSpaces::WorldSpace);
				if (!Pelvis.ContainsNaN())
				{
					// bEnableCameraLag (10.0 on BP_Player) smooths the follow for free.
					Arm->SetWorldLocation(Pelvis);
				}
			}
		}
	}
}
