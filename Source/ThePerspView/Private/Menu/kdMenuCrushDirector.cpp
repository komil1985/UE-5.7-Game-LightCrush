// Copyright ASKD Games

#include "Menu/kdMenuCrushDirector.h"
#include "Menu/kdMenuPresentationSubsystem.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Curves/CurveFloat.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

UkdMenuCrushDirector::UkdMenuCrushDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UkdMenuCrushDirector::BeginPlay()
{
	Super::BeginPlay();

	// Guarantee a valid, normalized collapse axis even if it was zeroed in editor.
	CollapseAxis = CollapseAxis.GetSafeNormal();
	if (CollapseAxis.IsNearlyZero())
	{
		CollapseAxis = FVector::ForwardVector;
	}

	CacheRestState();

	// Register as the active menu director so the title-word buttons can find us
	// through the world subsystem, with no hard widget->actor coupling.
	if (UWorld* World = GetWorld())
	{
		if (UkdMenuPresentationSubsystem* Sub = World->GetSubsystem<UkdMenuPresentationSubsystem>())
		{
			Sub->RegisterDirector(this);
		}
	}

	// The pawn is spawned/possessed from the Player Start and may not exist yet on
	// this BeginPlay. Defer one tick to resolve its boom and capture the Light pose,
	// so the seed isn't taken from a half-initialized rig. ApplySpringArm also
	// resolves lazily, so a slower possession still recovers on the first transition.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					ApplySpringArm(CurrentAlpha);
				}));
	}

	// Open in the Light (uncrushed) state.
	CurrentAlpha = 0.f;
	CommittedTarget = 0.f;
	ApplyAlphaToTargets(0.f);
	ApplyPostProcess(0.f);
	ApplySpringArm(0.f);
	WriteColorParameters(0.f, 0.f);
}

void UkdMenuCrushDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld())
	{
		if (UkdMenuPresentationSubsystem* Sub = World->GetSubsystem<UkdMenuPresentationSubsystem>())
		{
			Sub->UnregisterDirector(this);
		}
	}

	Super::EndPlay(Reason);
}

void UkdMenuCrushDirector::CacheRestState()
{
	RestScales.Reset();
	RestScales.Reserve(CrushTargets.Num());

	for (const TObjectPtr<AActor>& Actor : CrushTargets)
	{
		FVector Rest = FVector::OneVector;

		if (IsValid(Actor) && Actor->GetRootComponent())
		{
			// A Static-mobility root cannot be transformed at runtime -- warn loudly
			// so it is caught in PIE rather than as a silent "nothing animates" bug.
			if (Actor->GetRootComponent()->Mobility == EComponentMobility::Static)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[kdMenuCrushDirector] '%s' root is Static mobility and will NOT animate. Set it to Movable."),
					*Actor->GetName());
			}

			Rest = Actor->GetRootComponent()->GetRelativeScale3D();
		}

		RestScales.Add(Rest);
	}
}

void UkdMenuCrushDirector::ApplyAlphaToTargets(float Alpha) const
{
	// Build a per-axis thickness multiplier: components aligned with CollapseAxis
	// shrink toward CrushedThickness, perpendicular components stay at 1.0. This
	// lets a diagonal axis behave sensibly while a pure cardinal axis is exact.
	const FVector AxisAbs(FMath::Abs(CollapseAxis.X), FMath::Abs(CollapseAxis.Y), FMath::Abs(CollapseAxis.Z));

	const FVector ThicknessMul(
		FMath::Lerp(1.f, CrushedThickness, AxisAbs.X),
		FMath::Lerp(1.f, CrushedThickness, AxisAbs.Y),
		FMath::Lerp(1.f, CrushedThickness, AxisAbs.Z));

	const FVector FrameMul = FMath::Lerp(FVector::OneVector, ThicknessMul, Alpha);

	for (int32 i = 0; i < CrushTargets.Num(); ++i)
	{
		const TObjectPtr<AActor>& Actor = CrushTargets[i];
		if (!IsValid(Actor))
		{
			continue;
		}

		USceneComponent* Root = Actor->GetRootComponent();
		if (!Root)
		{
			continue;
		}

		const FVector Rest = RestScales.IsValidIndex(i) ? RestScales[i] : FVector::OneVector;
		Root->SetRelativeScale3D(Rest * FrameMul);
	}
}

void UkdMenuCrushDirector::ApplyPostProcess(float Alpha) const
{
	// Crossfade the two placed volumes by their BlendWeight. We mutate ONLY
	// BlendWeight (and ensure they are enabled) -- the M_CrushPostProcess blendable
	// and every other authored setting on each volume are left untouched.
	if (IsValid(LightVolume))
	{
		LightVolume->bEnabled = true;
		LightVolume->BlendWeight = bCrossfadeVolumes ? (1.f - Alpha) : 1.f;
	}

	if (IsValid(ShadowVolume))
	{
		ShadowVolume->bEnabled = true;
		ShadowVolume->BlendWeight = Alpha;
	}
}

USpringArmComponent* UkdMenuCrushDirector::ResolveSpringArm()
{
	if (CachedArm.IsValid())
	{
		return CachedArm.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Primary path: the menu display pawn is a copy that nothing possesses, so it
	// cannot be reached via the player controller. Locate it by its actor tag and
	// take the first spring arm on it.
	if (!MenuPawnTag.IsNone())
	{
		TArray<AActor*> Tagged;
		UGameplayStatics::GetAllActorsWithTag(World, MenuPawnTag, Tagged);
		for (AActor* Actor : Tagged)
		{
			if (IsValid(Actor))
			{
				if (USpringArmComponent* Arm = Actor->FindComponentByClass<USpringArmComponent>())
				{
					CachedArm = Arm;
					return Arm;
				}
			}
		}
	}

	// Fallback: a possessed pawn, in case this menu is ever wired that way.
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		if (USpringArmComponent* Arm = Pawn->FindComponentByClass<USpringArmComponent>())
		{
			CachedArm = Arm;
			return Arm;
		}
	}

	return nullptr;
}

void UkdMenuCrushDirector::ApplySpringArm(float Alpha)
{
	USpringArmComponent* Arm = ResolveSpringArm();
	if (!Arm)
	{
		return; // Pawn not spawned/possessed yet; a later apply will pick it up.
	}

	// Capture the Light pose from the live arm the first time it resolves.
	if (bSeedLightRotationFromArm && !bLightRotationSeeded)
	{
		LightArmRotation = Arm->GetRelativeRotation();
		bLightRotationSeeded = true;
	}

	// Slerp between the two poses. FQuat::Slerp takes the shortest arc and never
	// gimbals, so a large pitch delta stays smooth and pop-free (naive rotator lerp
	// is exactly what caused the in-game transition snaps).
	const FQuat A = LightArmRotation.Quaternion();
	const FQuat B = CrushArmRotation.Quaternion();
	const FQuat R = FQuat::Slerp(A, B, FMath::Clamp(Alpha, 0.f, 1.f));

	Arm->SetRelativeRotation(R.Rotator());
}

void UkdMenuCrushDirector::WriteColorParameters(float Alpha, float PulseAlpha) const
{
	if (!IsValid(WorldColorMPC))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UKismetMaterialLibrary::SetScalarParameterValue(World, WorldColorMPC, P_WorldBlendAlpha, Alpha);
	UKismetMaterialLibrary::SetScalarParameterValue(World, WorldColorMPC, P_ShadowTintAlpha, Alpha * MaxShadowTint);
	UKismetMaterialLibrary::SetScalarParameterValue(World, WorldColorMPC, P_EdgePulseAlpha, PulseAlpha);
}

float UkdMenuCrushDirector::EvaluateEase(float Linear01) const
{
	Linear01 = FMath::Clamp(Linear01, 0.f, 1.f);

	if (IsValid(CrushCurve))
	{
		// Curve maps normalized progress -> eased progress; evaluated exactly once.
		return CrushCurve->GetFloatValue(Linear01);
	}

	return FMath::SmoothStep(0.f, 1.f, Linear01);
}

void UkdMenuCrushDirector::BeginTransition(float NewTarget)
{
	StartAlpha = CurrentAlpha;
	CommittedTarget = FMath::Clamp(NewTarget, 0.f, 1.f);
	TransitionElapsed = 0.f;
	bTransitioning = true;
	PulseRemaining = EdgePulseDuration; // Fire the edge pulse on every state change.
}

void UkdMenuCrushDirector::SetCrushed(bool bCrushed, bool bImmediate)
{
	bUserHasInteracted = true; // Permanently disables attract mode for this session.

	const float Goal = bCrushed ? 1.f : 0.f;

	if (bImmediate)
	{
		bTransitioning = false;
		CommittedTarget = Goal;
		CurrentAlpha = Goal;
		PulseRemaining = 0.f;
		ApplyAlphaToTargets(Goal);
		ApplyPostProcess(Goal);
		ApplySpringArm(Goal);
		WriteColorParameters(Goal, 0.f);
		return;
	}

	BeginTransition(Goal);
}

void UkdMenuCrushDirector::ToggleCrush()
{
	SetCrushed(!IsCrushed());
}

void UkdMenuCrushDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Edge pulse decays independently of the transition so it can tail off after
	// the geometry has settled, giving a clean one-shot ripple on every toggle.
	float PulseAlpha = 0.f;
	if (PulseRemaining > 0.f)
	{
		PulseRemaining = FMath::Max(0.f, PulseRemaining - DeltaTime);
		PulseAlpha = (EdgePulseDuration > 0.f) ? (PulseRemaining / EdgePulseDuration) : 0.f;
	}

	// Attract mode: smoothly oscillate until the player first interacts, then hand
	// full control to them for the rest of the session.
	if (bEnableAttractMode && !bUserHasInteracted)
	{
		AttractPhase += DeltaTime * AttractCyclesPerSecond * 2.f * PI;
		CurrentAlpha = 0.5f - 0.5f * FMath::Cos(AttractPhase); // smooth 0..1..0
		ApplyAlphaToTargets(CurrentAlpha);
		ApplyPostProcess(CurrentAlpha);
		ApplySpringArm(CurrentAlpha);
		WriteColorParameters(CurrentAlpha, PulseAlpha);
		return;
	}

	if (bTransitioning)
	{
		TransitionElapsed += DeltaTime;

		const float Linear = (TransitionDuration > 0.f) ? (TransitionElapsed / TransitionDuration) : 1.f;
		const float Eased = EvaluateEase(Linear);
		CurrentAlpha = FMath::Lerp(StartAlpha, CommittedTarget, Eased);

		if (Linear >= 1.f)
		{
			CurrentAlpha = CommittedTarget;
			bTransitioning = false;
		}

		ApplyAlphaToTargets(CurrentAlpha);
		ApplyPostProcess(CurrentAlpha);
		ApplySpringArm(CurrentAlpha);
		WriteColorParameters(CurrentAlpha, PulseAlpha);
		return;
	}

	// Settled: keep refreshing only while the edge pulse is still tailing off.
	if (PulseAlpha > 0.f)
	{
		WriteColorParameters(CurrentAlpha, PulseAlpha);
	}
}