// Copyright ASKD Games

#include "Components/kdRagdollComponent.h"
#include "Components/kdPlayerHoverComponent.h"
#include "Player/kdMyPlayer.h"
#include "Crush/kdCrushDirectionLibrary.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

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

	CachedHover = CachedPlayer->FindComponentByClass<UkdPlayerHoverComponent>();

	// Capture the canonical baselines BEFORE any tick runs. Component BeginPlay is
	// guaranteed to complete for every component before the first TickComponent,
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
}

FName UkdRagdollComponent::ResolvePelvisBone(USkeletalMeshComponent* Mesh) const
{
	if (!Mesh) return NAME_None;

	if (Mesh->GetBoneIndex(PelvisBoneName) != INDEX_NONE)
	{
		return PelvisBoneName;
	}

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
	if (bRagdolling)
	{
		UE_LOG(LogTemp, Warning, TEXT("[kdRagdoll] Already ragdolling — ignored."));
		return;
	}
	if (!IsValid(CachedPlayer))
	{
		UE_LOG(LogTemp, Error, TEXT("[kdRagdoll] CachedPlayer invalid — BeginPlay never resolved the owner."));
		return;
	}

	USkeletalMeshComponent* Mesh = CachedPlayer->GetMesh();
	if (!IsValid(Mesh))
	{
		UE_LOG(LogTemp, Error, TEXT("[kdRagdoll] Owner has no SkeletalMeshComponent."));
		return;
	}

	// ── 0. Physics Asset gate ────────────────────────────────────────────────
	// No asset = no bodies = SetSimulatePhysics is a silent no-op that leaves the
	// mesh frozen mid-air. Fail loud; the rest of the death sequence still runs.
	if (!Mesh->GetPhysicsAsset())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[kdRagdoll] Skeletal Mesh '%s' has NO Physics Asset — ragdoll skipped."),
			*GetNameSafe(Mesh->GetSkeletalMeshAsset()));
		return;
	}

	// ── 1. Scale sanity ──────────────────────────────────────────────────────
	// Chaos bakes body scale at simulation start and does not handle non-uniform
	// scale on convex/sphyl bodies — they interpenetrate and blow up.
	// UkdCrushTransitionComponent's Z squash-stretch produces exactly that
	// mid-morph. UkdDeathComponent aborts the transition before calling us; this
	// is the last line of defence.
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

	// ── 2. Seize mesh-transform authority from the cosmetic drivers ──────────
	// UkdJumpSquashComponent self-suspends on State.Dead (already applied by
	// UkdDeathComponent before this call). UkdPlayerHoverComponent has no ASC by
	// design, and writes mesh relative location EVERY frame — it is told directly.
	if (CachedHover)
	{
		CachedHover->SetSuspended(true);
	}

	// ── 3. Hand off the capsule ──────────────────────────────────────────────
	if (UCharacterMovementComponent* MC = CachedPlayer->GetCharacterMovement())
	{
		MC->StopMovementImmediately();
		MC->DisableMovement();   // MOVE_None — the CMC stops writing the capsule
	}

	if (UCapsuleComponent* Capsule = CachedPlayer->GetCapsuleComponent())
	{
		SavedCapsuleCollision = Capsule->GetCollisionEnabled();
		// Off, or the capsule ploughs the corpse around and blocks its own fall.
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// ── 4. Collision — inherit the CAPSULE's contract, do not invent one ──────
	// The capsule has navigated this world correctly all game: floors block it,
	// hazards OVERLAP it — and that overlap is literally what fires
	// HandlePlayerDeath. Two earlier versions got this wrong in instructive ways:
	//   • The engine "Ragdoll" preset switches object type to PhysicsBody. Floors
	//     that only block Pawn then swallow the corpse.  → fall-through.
	//   • SetCollisionResponseToAllChannels(ECR_Block) made the corpse block the
	//     hazard it was standing inside. Chaos ejects it from an angled cone face;
	//     an angled contact is torque.  → spin.
	SavedMeshRelScale = Scale;
	SavedMeshProfile = Mesh->GetCollisionProfileName();
	SavedMeshCameraResponse = Mesh->GetCollisionResponseToChannel(ECC_Camera);

	if (const UCapsuleComponent* Capsule = CachedPlayer->GetCapsuleComponent())
	{
		Mesh->SetCollisionObjectType(Capsule->GetCollisionObjectType());
		Mesh->SetCollisionResponseToChannels(Capsule->GetCollisionResponseToChannels());
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);  // SpringArm probe
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);  // shadow traces

#if !UE_BUILD_SHIPPING
	LogFloor(Mesh);   // BEFORE sim starts — captures the spawn condition
#endif

	// ── 5. Simulate ──────────────────────────────────────────────────────────
	Mesh->SetSimulatePhysics(true);
	Mesh->SetAllBodiesPhysicsBlendWeight(1.f);
	Mesh->SetAllUseCCD(true);

	FVector StartVel = InheritedVelocity * VelocityInheritance;
	if (MaxInheritedSpeed > KINDA_SMALL_NUMBER)
	{
		StartVel = StartVel.GetClampedToMaxSize(MaxInheritedSpeed);
	}
	Mesh->SetAllPhysicsLinearVelocity(StartVel);
	Mesh->WakeAllRigidBodies();

	// ── 6. Crush-plane lock ──────────────────────────────────────────────────
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

#if !UE_BUILD_SHIPPING
	LogRig(Mesh);
#endif
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
		// codebase re-derives it, so restoring the baseline here is mandatory or
		// the mesh is permanently offset after the first death.
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

	// JumpSquash resumes on its own when State.Dead clears in ClearAllGASDeathState.
	if (CachedHover)
	{
		CachedHover->SetSuspended(false);
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[kdRagdoll] EXIT — mesh re-anchored to capsule."));
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// TickComponent — cosmetics only. Camera follow and plane lock.
// ─────────────────────────────────────────────────────────────────────────────
void UkdRagdollComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bRagdolling || !IsValid(CachedPlayer)) { SetComponentTickEnabled(false); return; }

	USkeletalMeshComponent* Mesh = CachedPlayer->GetMesh();
	if (!IsValid(Mesh)) { SetComponentTickEnabled(false); return; }

	// ── Hold the corpse on the shadow plane ──────────────────────────────────
	// Velocity projection only — snapping body transforms would fight the solver.
	if (bPlaneLockActive)
	{
		for (FBodyInstance* Body : Mesh->Bodies)
		{
			// bSimulatePhysics, not IsInstanceSimulatingPhysics(): that inline
			// forwards to FBodyInstanceCore::ShouldInstanceSimulatingPhysics(),
			// which is PHYSICSCORE_API — visible to the compiler, not linked to
			// this module. The bitfield read is header-only.
			if (!Body || !Body->bSimulatePhysics) continue;

			const FVector V = Body->GetUnrealWorldVelocity();
			Body->SetLinearVelocity(FVector::VectorPlaneProject(V, LockedCollapseNormal), false);
		}
	}

	// ── Keep the camera on the body ──────────────────────────────────────────
	// Move the SPRING ARM. Never the actor.
	//
	// The original version called SetActorLocation(..., ETeleportType::TeleportPhysics).
	// TeleportPhysics is precisely the flag that makes UpdateKinematicBonesToAnim
	// force-move bodies EVEN WHEN SIMULATING — so every frame the corpse was
	// teleported onto the capsule, the solver differenced that jump against its
	// own integration and turned the delta into velocity. It fed back on itself:
	// capsule chases pelvis → teleport moves pelvis → capsule chases again.
	//
	// The SpringArm is not a physics body, and UkdCrushTransitionComponent writes
	// only its rotation and TargetArmLength — never its location.
	if (bCameraFollowsRagdoll)
	{
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

// ─────────────────────────────────────────────────────────────────────────────
// Diagnostics
// ─────────────────────────────────────────────────────────────────────────────
#if !UE_BUILD_SHIPPING

void UkdRagdollComponent::LogRig(USkeletalMeshComponent* Mesh) const
{
	if (!Mesh) return;

	const int32 Bodies = Mesh->Bodies.Num();
	const int32 Constraints = Mesh->Constraints.Num();
	const bool  bSim = Mesh->IsSimulatingPhysics();

	// Warning verbosity: yellow and unmissable. These four facts decide whether a
	// bad-looking death is a code problem or an asset problem, and every previous
	// debugging round was lost to not having them in one line.
	UE_LOG(LogTemp, Warning,
		TEXT("[kdRagdoll] RIG | asset=%s  bones=%d  bodies=%d  constraints=%d  mass=%.2fkg  scale=%.2f  sim=%s%s"),
		*GetNameSafe(Mesh->GetPhysicsAsset()),
		Mesh->GetNumBones(), Bodies, Constraints,
		Mesh->GetMass(), Mesh->GetComponentScale().X,
		bSim ? TEXT("YES") : TEXT("NO — body Physics Type is probably Kinematic"),
		(Constraints == 0)
		? TEXT("   <-- 0 CONSTRAINTS: this is a rigid marble, not a ragdoll. It CANNOT flop. Rig the mesh.")
		: TEXT(""));
}

void UkdRagdollComponent::LogFloor(USkeletalMeshComponent* Mesh) const
{
	if (!Mesh || !GetWorld() || !CachedPlayer) return;

	const FVector Origin = Mesh->GetComponentLocation();
	const ECollisionChannel Ch = Mesh->GetCollisionObjectType();

	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(kdRagdollFloorProbe), /*bTraceComplex=*/false, CachedPlayer);

	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin - FVector(0.f, 0.f, 5000.f), Ch, P))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[kdRagdoll] FLOOR | nothing below within 50m on the corpse's channel — void death."));
		return;
	}

	// The question a line trace CANNOT answer on its own:
	// rigid bodies collide against SIMPLE collision only, while line traces also
	// hit complex (per-triangle) collision. So a floor with no simple primitives,
	// or one set to UseComplexAsSimple, reports a clean BLOCK to a trace and is
	// completely invisible to the corpse. That is a fall-through no amount of
	// ragdoll tuning can ever fix, and it is not visible from the Details panel.
	const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Hit.GetComponent());
	const UStaticMesh* SM = SMC ? SMC->GetStaticMesh() : nullptr;
	const UBodySetup* BS = SM ? SM->GetBodySetup() : nullptr;

	if (!BS)
	{
		UE_LOG(LogTemp, Log, TEXT("[kdRagdoll] FLOOR | '%s' — not a static mesh, skipping simple-collision check."),
			*GetNameSafe(Hit.GetActor()));
		return;
	}

	const int32 SimpleElems = BS->AggGeom.GetElementCount();
	const bool  bComplexAsSimple = (BS->CollisionTraceFlag == CTF_UseComplexAsSimple);
	const bool  bBroken = (SimpleElems == 0) || bComplexAsSimple;

	// UE_LOG needs a compile-time verbosity, so the branch is on the call, not the arg.
	if (bBroken)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[kdRagdoll] FLOOR | actor='%s'  mesh='%s'  simplePrimitives=%d  complexity=%s"
				"   <-- NO SIMPLE COLLISION: rigid bodies pass straight through this. "
				"Fix: open the mesh -> Collision -> Add Box Simplified Collision, and set "
				"Collision Complexity = Default."),
			*GetNameSafe(Hit.GetActor()), *GetNameSafe(SM), SimpleElems,
			bComplexAsSimple ? TEXT("UseComplexAsSimple") : TEXT("Default"));
	}
	else
	{
		UE_LOG(LogTemp, Log,
			TEXT("[kdRagdoll] FLOOR | actor='%s'  mesh='%s'  simplePrimitives=%d  complexity=Default   (ok)"),
			*GetNameSafe(Hit.GetActor()), *GetNameSafe(SM), SimpleElems);
	}
}

#endif // !UE_BUILD_SHIPPING