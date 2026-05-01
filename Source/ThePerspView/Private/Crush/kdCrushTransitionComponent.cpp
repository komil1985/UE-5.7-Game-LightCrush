// Copyright ASKD Games


#include "Crush/kdCrushTransitionComponent.h"
#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TimelineComponent.h"

UkdCrushTransitionComponent::UkdCrushTransitionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	CrushTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("CrushTimeline"));
}

void UkdCrushTransitionComponent::BeginPlay()
{
	Super::BeginPlay();

	//CachedOwner = CastChecked<AkdMyPlayer>(GetOwner());
	//PlayerOriginalScale = FVector(0.2f, 0.2f, 0.2f);

	//if (TransitionCurve)
	//{
	//	FOnTimelineFloat UpdateDelegate;
	//	UpdateDelegate.BindUFunction(this, FName("HandleTimelineUpdate"));
	//	CrushTimeline->AddInterpFloat(TransitionCurve, UpdateDelegate);

	//	FOnTimelineEvent FinishedDelegate;
	//	FinishedDelegate.BindUFunction(this, FName("HandleTimelineFinished"));
	//	CrushTimeline->SetTimelineFinishedFunc(FinishedDelegate);
	//}

	CachedOwner = CastChecked<AkdMyPlayer>(GetOwner());

	// ── Capture stable 3D baselines ───────────────────────────────────────────
	// These must be read BEFORE any transition runs so returning to 3D always
	// restores the true original values, regardless of how many toggles occur.

	if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
	{
		Original3DScale = Mesh->GetRelativeScale3D();
	}

	if (CachedOwner->Camera)
	{
		Original3DOrthoWidth = CachedOwner->Camera->OrthoWidth;
	}

	if (CachedOwner->SpringArm)
	{
		Original3DArmRotation = CachedOwner->SpringArm->GetRelativeRotation();
	}

	// ── Wire timeline delegates ───────────────────────────────────────────────
	if (!TransitionCurve)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UkdCrushTransitionComponent [%s]: No TransitionCurve assigned — "
				"transitions will use a linear ramp."),
			*GetOwner()->GetName());
	}

	FOnTimelineFloat UpdateDelegate;
	UpdateDelegate.BindUFunction(this, FName("HandleTimelineUpdate"));
	CrushTimeline->AddInterpFloat(TransitionCurve, UpdateDelegate);

	FOnTimelineEvent FinishedDelegate;
	FinishedDelegate.BindUFunction(this, FName("HandleTimelineFinished"));
	CrushTimeline->SetTimelineFinishedFunc(FinishedDelegate);

}

void UkdCrushTransitionComponent::StartTransition(bool bToCrushMode)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("CrushTransition [%s]: StartTransition → %s"),
		*GetOwner()->GetName(),
		bToCrushMode ? TEXT("2D (Crush)") : TEXT("3D (Normal)"));
#endif

	if (!CachedOwner)
	{
		UE_LOG(LogTemp, Error, TEXT("CrushTransition: CachedOwner is null — transition aborted."));
		return;
	}
	if (!TransitionCurve) return;

	//bTargetCrushMode = bToCrushMode;

	//InitialScale = CachedOwner->GetMesh()->GetRelativeScale3D();							// Cache starting scale values
	//TargetScaleCache = bTargetCrushMode ? PlayerCrushScale : PlayerOriginalScale;			// Determine Target Scale

	//InitialOrthoWidth = CachedOwner->Camera->OrthoWidth;
	//TargetOrthoWidthCache = bTargetCrushMode ? CrushOrthoWidth : InitialOrthoWidth;

	//CrushTimeline->SetPlayRate(1.0f / TransitionDuration);
	//CrushTimeline->PlayFromStart();
	
	bTargetCrushMode = bToCrushMode;
	bProjectionSwitchDone = false;   // reset the midpoint-switch guard

	// ── Snapshot current values to lerp FROM ──────────────────────────────────

	if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
	{
		InitialScale = Mesh->GetRelativeScale3D();
	}

	if (CachedOwner->Camera)
	{
		InitialOrthoWidth = CachedOwner->Camera->OrthoWidth;
	}

	// ── Resolve target values to lerp TO ─────────────────────────────────────
	//
	// BUG FIX: Previously, when exiting crush mode the target was set to
	// InitialOrthoWidth (the current crush width), making the lerp a no-op.
	// We now always target Original3DOrthoWidth when returning to 3D.

	TargetScaleCache = bTargetCrushMode ? PlayerCrushScale : Original3DScale;
	TargetOrthoWidthCache = bTargetCrushMode ? CrushOrthoWidth : Original3DOrthoWidth;

	// Play rate converts normalised [0,1] curve time into real seconds.
	CrushTimeline->SetPlayRate(1.0f / FMath::Max(TransitionDuration, KINDA_SMALL_NUMBER));
	CrushTimeline->PlayFromStart();
}

void UkdCrushTransitionComponent::HandleTimelineUpdate(float Value)
{
	//if (!CachedOwner) return;

	//// In Update Loop
	//float CurveValue = TransitionCurve ? TransitionCurve->GetFloatValue(Value) : Value;

	//// Curve value drives interpolation
	//FVector NewScale = FMath::Lerp(InitialScale, TargetScaleCache, CurveValue);
	//CachedOwner->GetMesh()->SetRelativeScale3D(NewScale);

	//float NewOrthoWidth = FMath::Lerp(InitialOrthoWidth, TargetOrthoWidthCache, CurveValue);
	//CachedOwner->Camera->SetOrthoWidth(NewOrthoWidth);

	//// Switch projection at midpoint
	//if (Value >= 0.5f)
	//{
	//	if (bTargetCrushMode && CachedOwner->Camera->ProjectionMode != ECameraProjectionMode::Orthographic)
	//	{
	//		CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	//		CachedOwner->Camera->SetOrthoWidth(NewOrthoWidth);
	//		CachedOwner->Camera->bAutoCalculateOrthoPlanes = false;
	//		CachedOwner->SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
	//		CachedOwner->SpringArm->bInheritYaw = false;
	//	}
	//	else if (!bTargetCrushMode && CachedOwner->Camera->ProjectionMode != ECameraProjectionMode::Perspective)
	//	{
	//		CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
	//		CachedOwner->SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));
	//		CachedOwner->SpringArm->bInheritYaw = true;
	//	}
	//}

	if (!CachedOwner) return;

	// `Value` is the curve-evaluated alpha — use it directly.
	const float Alpha = Value;

	// ── Mesh scale lerp ───────────────────────────────────────────────────────
	if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
	{
		Mesh->SetRelativeScale3D(FMath::Lerp(InitialScale, TargetScaleCache, Alpha));
	}

	// ── OrthoWidth lerp ───────────────────────────────────────────────────────
	if (CachedOwner->Camera)
	{
		const float NewOrthoWidth = FMath::Lerp(InitialOrthoWidth, TargetOrthoWidthCache, Alpha);
		CachedOwner->Camera->SetOrthoWidth(NewOrthoWidth);
	}

	// ── Midpoint camera projection switch (fires exactly once per transition) ─
	//
	// BUG FIX 1: Previously the branch re-ran every tick once Alpha >= 0.5,
	//            wasting evaluations and risking edge-case double-sets.
	//            bProjectionSwitchDone now ensures a single fire.
	//
	// BUG FIX 2: Previously SetOrthoWidth was called a second time inside the
	//            switch block — now the lerp above is the only call site.

	if (!bProjectionSwitchDone && Alpha >= 0.5f)
	{
		bProjectionSwitchDone = true;

		if (bTargetCrushMode)
		{
			// ── Switch to Orthographic (entering 2D crush mode) ───────────────
			CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
			CachedOwner->Camera->bAutoCalculateOrthoPlanes = false;

			if (CachedOwner->SpringArm)
			{
				CachedOwner->SpringArm->SetRelativeRotation(Crush2DArmRotation);
				CachedOwner->SpringArm->bInheritYaw = false;
			}
		}
		else
		{
			// ── Switch to Perspective (returning to 3D) ───────────────────────
			CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Perspective);

			if (CachedOwner->SpringArm)
			{
				CachedOwner->SpringArm->SetRelativeRotation(Original3DArmRotation);
				CachedOwner->SpringArm->bInheritYaw = true;
			}
		}

#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("CrushTransition: Projection switched to %s at alpha=%.2f"),
			bTargetCrushMode ? TEXT("Orthographic") : TEXT("Perspective"), Alpha);
#endif
	}
}

void UkdCrushTransitionComponent::HandleTimelineFinished()
{
	if (OnTransitionComplete.IsBound())
	{
		OnTransitionComplete.Broadcast(bTargetCrushMode);
	}
}
