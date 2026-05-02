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
				"transitions will use a linear ramp."),	*GetOwner()->GetName());
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
	UE_LOG(LogTemp, Log, TEXT("CrushTransition [%s]: StartTransition → %s"), *GetOwner()->GetName(), bToCrushMode ? TEXT("2D (Crush)") : TEXT("3D (Normal)"));
#endif

	if (!CachedOwner)
	{
		UE_LOG(LogTemp, Error, TEXT("CrushTransition: CachedOwner is null — transition aborted."));
		return;
	}
	if (!TransitionCurve) return;

	// ── Cancel any in-progress transition cleanly ─────────────────────────────
	GetWorld()->GetTimerManager().ClearTimer(AnticipationTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
	CrushTimeline->Stop();

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

	// always target Original3DOrthoWidth when returning to 3D.
	TargetScale = bTargetCrushMode ? PlayerCrushScale : Original3DScale;
	TargetOrthoWidth = bTargetCrushMode ? CrushOrthoWidth : Original3DOrthoWidth;

	// ── PHASE 1: Anticipation scale punch ─────────────────────────────────────
// Entering Crush → brief scale UP  (cartoon "breath in" before squashing flat).
// Exiting  Crush → brief scale DOWN (compress before expanding back to 3D).
//
// The punched mesh scale becomes the START of the main morph lerp so the
// animation feels like one continuous motion rather than punch → reset → lerp.
//
// Formula: punch only applies to the mesh scale; OrthoWidth lerp starts from
// its current value unchanged (the camera doesn't punch, only the character).

	if (AnticipationDuration > KINDA_SMALL_NUMBER && AnticipationScalePunch > KINDA_SMALL_NUMBER)
	{
		if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
		{
			const FVector CurrentMeshScale = Mesh->GetRelativeScale3D();

			// Entering crush: +punch  |  Exiting crush: -half punch (subtler)
			const float PunchFraction = bTargetCrushMode ? AnticipationScalePunch : -AnticipationScalePunch * 0.5f;
			Mesh->SetRelativeScale3D(CurrentMeshScale * (1.f + PunchFraction));
		}

		GetWorld()->GetTimerManager().SetTimer(AnticipationTimerHandle, this, &UkdCrushTransitionComponent::BeginMainTimeline, AnticipationDuration, false);
	}
	else
	{
		BeginMainTimeline();
	}

	// Play rate converts normalised [0,1] curve time into real seconds.
	//CrushTimeline->SetPlayRate(1.0f / FMath::Max(TransitionDuration, KINDA_SMALL_NUMBER));
	//CrushTimeline->PlayFromStart();
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
		Mesh->SetRelativeScale3D(FMath::Lerp(InitialScale, TargetScale, Alpha));
	}

	// ── OrthoWidth lerp ───────────────────────────────────────────────────────
	if (CachedOwner->Camera)
	{
		const float NewOrthoWidth = FMath::Lerp(InitialOrthoWidth, TargetOrthoWidth, Alpha);
		CachedOwner->Camera->SetOrthoWidth(NewOrthoWidth);
	}

	// ── Midpoint camera projection switch (fires exactly once per transition) ─
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
	//if (OnTransitionComplete.IsBound())
	//{
	//	OnTransitionComplete.Broadcast(bTargetCrushMode);
	//}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("CrushTransition: Morph finished (bCrushMode=%d). "
		"Settle: overshoot=%.2f duration=%.2fs"),
		bTargetCrushMode, SettleOvershootAmount, SettleDuration);
#endif

	// ── Gameplay state update — fires immediately, not delayed by settle ───────
	OnTransitionComplete.Broadcast(bTargetCrushMode);

	// ── PHASE 3: Settle overshoot ────────────────────────────────────────────
	// Scale jumps past the target by SettleOvershootAmount then decays back.
	// This gives the morph a satisfying "pop" rather than a limp stop.
	if (SettleOvershootAmount > KINDA_SMALL_NUMBER && SettleDuration > KINDA_SMALL_NUMBER)
	{
		if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
		{
			const FVector OvershootScale = TargetScale * (1.f + SettleOvershootAmount);
			Mesh->SetRelativeScale3D(OvershootScale);
		}

		SettleElapsed = 0.f;
		GetWorld()->GetTimerManager().SetTimer(SettleTickHandle, this, &UkdCrushTransitionComponent::SettleTick, 1.f / 60.f, true); // ~60 Hz looping
	}
}

void UkdCrushTransitionComponent::BeginMainTimeline()
{
	if (!CachedOwner) return;

	// Snapshot current mesh scale as the lerp start — this captures the punch
	// so the main morph continues from where anticipation left off.
	if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
	{
		InitialScale = Mesh->GetRelativeScale3D();
	}
	if (CachedOwner->Camera)
	{
		InitialOrthoWidth = CachedOwner->Camera->OrthoWidth;
	}

	bProjectionSwitchDone = false;

	CrushTimeline->SetPlayRate(1.0f / FMath::Max(TransitionDuration, KINDA_SMALL_NUMBER));
	CrushTimeline->PlayFromStart();
}

void UkdCrushTransitionComponent::SettleTick()
{
	if (!CachedOwner) return;

	constexpr float kTickRate = 1.f / 60.f;
	SettleElapsed += kTickRate;

	const float t = FMath::Clamp(SettleElapsed / FMath::Max(SettleDuration, kTickRate), 0.f, 1.f);

	// Exponential ease: fast initial snap, smooth tail — feels springy.
	// EaseAlpha reaches ~0.996 at t=1 so the final frame snaps cleanly to target.
	const float EaseAlpha = 1.f - FMath::Exp(-6.f * t);

	USkeletalMeshComponent* Mesh = CachedOwner->GetMesh();
	if (!Mesh)
	{
		GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
		return;
	}

	if (t >= 1.f)
	{
		// Snap to exact target — eliminates floating-point drift.
		Mesh->SetRelativeScale3D(TargetScale);
		GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
		return;
	}

	const FVector OvershootScale = TargetScale * (1.f + SettleOvershootAmount);
	Mesh->SetRelativeScale3D(FMath::Lerp(OvershootScale, TargetScale, EaseAlpha));
}
