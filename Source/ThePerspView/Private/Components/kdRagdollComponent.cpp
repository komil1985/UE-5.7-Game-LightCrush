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
#include "PhysicsEngine/SkeletalBodySetup.h"



#if !UE_BUILD_SHIPPING
// ─────────────────────────────────────────────────────────────────────────────
// GetAuthoredBodyReach — the collision shape's half-extent along Z, in world cm.
//
// NOT UE_BUILD_SHIPPING-guarded: ProbePlacement feeds the spawn lift, which is
// gameplay, not diagnostics. Guarding it would compile in DebugGame and break
// Shipping — the worst possible time to find out.
//
// Reads the AUTHORED primitive from the Physics Asset rather than
// Mesh->Bounds.SphereRadius. Bounds is the RENDER volume and can be far larger
// than the collision shape; that proxy is what reported a flat -30.0 earlier.
// ─────────────────────────────────────────────────────────────────────────────
static float GetAuthoredBodyReach(const USkeletalMeshComponent* Mesh, FName BoneName)
{
	const UPhysicsAsset* PA = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
	if (!PA) return 0.f;

	// Resolve the body that actually belongs to this bone. Index 0 happens to be
	// correct for your single-body sphere, but silently picks the wrong body the
	// day the asset gains a second one.
	int32 Index = PA->FindBodyIndex(BoneName);
	if (Index == INDEX_NONE)
	{
		if (PA->SkeletalBodySetups.Num() == 0) return 0.f;
		Index = 0;
	}

	const USkeletalBodySetup* BS = PA->SkeletalBodySetups[Index];
	if (!BS) return 0.f;

	const FKAggregateGeom& Agg = BS->AggGeom;
	float Reach = 0.f;

	for (const FKSphereElem& E : Agg.SphereElems)
	{
		Reach = FMath::Max(Reach, E.Radius + FMath::Abs(E.Center.Z));
	}
	for (const FKSphylElem& E : Agg.SphylElems)
	{
		Reach = FMath::Max(Reach, E.Radius + E.Length * 0.5f + FMath::Abs(E.Center.Z));
	}
	for (const FKBoxElem& E : Agg.BoxElems)
	{
		Reach = FMath::Max(Reach, E.Z * 0.5f + FMath::Abs(E.Center.Z));
	}

	// Uniform component scale is guaranteed by EnterRagdoll's AllComponentsEqual guard.
	return Reach * Mesh->GetComponentScale().X;
}
#endif


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

	// ── 4. Resolve the landing spot ONCE. No solver, no per-frame collision.
	const FName  Bone = ResolvePelvisBone(Mesh);
	const FVector Origin = Mesh->GetComponentLocation();
	const float   Reach = GetAuthoredBodyReach(Mesh, Bone);

	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(kdDeathDrop), false, CachedPlayer);

	// Trace on the CAPSULE's channel — the one that has demonstrably found this
	// floor every frame the player has been standing on it.
	const ECollisionChannel Ch = CachedPlayer->GetCapsuleComponent()
		? CachedPlayer->GetCapsuleComponent()->GetCollisionObjectType() : ECC_Pawn;

	DropStartLoc = Origin;

	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin - FVector(0, 0, 10000.f), Ch, P))
	{
		DropEndLoc = FVector(Origin.X, Origin.Y, Hit.ImpactPoint.Z + Reach);
		UE_LOG(LogTemp, Log, TEXT("[kdDrop] Landing on '%s' at Z=%.1f (fall %.1fcm)"),
			*GetNameSafe(Hit.GetActor()), DropEndLoc.Z, Origin.Z - DropEndLoc.Z);
	}
	else
	{
		// Void death: fall past the camera. Never "forever" — the fade owns the end.
		DropEndLoc = Origin - FVector(0, 0, 1500.f);
		UE_LOG(LogTemp, Log, TEXT("[kdDrop] No floor — falling into the void."));
	}

	DropElapsed = 0.f;
	SettleElapsed = 0.f;
	bLanded = false;

	bRagdolling = true;
	SetComponentTickEnabled(true);

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
UkdRagdollComponent::FkdRagdollPlacement
UkdRagdollComponent::ProbePlacement(USkeletalMeshComponent* Mesh) const
{
	FkdRagdollPlacement Out;
	if (!Mesh || !GetWorld() || !CachedPlayer) return Out;

	const FName  Bone = ResolvePelvisBone(Mesh);
	const FVector Origin = (Bone != NAME_None)
		? Mesh->GetBoneLocation(Bone, EBoneSpaces::WorldSpace)   // where the body WILL be
		: Mesh->GetComponentLocation();

	const float Reach = GetAuthoredBodyReach(Mesh, Bone);

	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(kdRagdollProbe), false, CachedPlayer);
	const ECollisionChannel Ch = Mesh->GetCollisionObjectType();

	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin - FVector(0, 0, 5000.f), Ch, P))
	{
		return Out;   // airborne over the void — nothing to lift out of
	}

	Out.bFoundFloor = true;
	Out.FloorName = GetNameSafe(Hit.GetActor());
	Out.Penetration = FMath::Max(0.f, Hit.ImpactPoint.Z - (Origin.Z - Reach));
	return Out;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// TickComponent — only alive between EnterRagdoll and ExitRagdoll.
// ─────────────────────────────────────────────────────────────────────────────
void UkdRagdollComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bRagdolling || !IsValid(CachedPlayer)) { SetComponentTickEnabled(false); return; }
	USkeletalMeshComponent* Mesh = CachedPlayer->GetMesh();
	if (!IsValid(Mesh)) { SetComponentTickEnabled(false); return; }

	if (!bLanded)
	{
		DropElapsed += DeltaTime;
		const float t = FMath::Clamp(DropElapsed / DropDuration, 0.f, 1.f);

		// t² = constant acceleration. Reads as gravity because it IS gravity's curve.
		Mesh->SetWorldLocation(FMath::Lerp(DropStartLoc, DropEndLoc, t * t));

		if (t >= 1.f) { bLanded = true; }
		return;
	}

	// ── Impact squash → settle. This is the "dead body" read: the blob hits,
	//    flattens, and relaxes. One bone can't flop, but it can absolutely land
	//    with weight — and weight is what you actually asked for.
	SettleElapsed += DeltaTime;
	const float s = FMath::Clamp(SettleElapsed / FMath::Max(SettleDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float Amount = ImpactSquash * (1.f - s) * (1.f - s);   // decays out

	Mesh->SetRelativeScale3D(FVector(
		SavedMeshRelScale.X * (1.f + Amount * 0.5f),
		SavedMeshRelScale.Y * (1.f + Amount * 0.5f),
		SavedMeshRelScale.Z * (1.f - Amount)));

	if (s >= 1.f) { SetComponentTickEnabled(false); }
}
