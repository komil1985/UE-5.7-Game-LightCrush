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

	CachedOwner = CastChecked<AkdMyPlayer>(GetOwner());

	if (TransitionCurve)
	{
		FOnTimelineFloat UpdateDelegate;
		UpdateDelegate.BindUFunction(this, FName("HandleTimelineUpdate"));
		CrushTimeline->AddInterpFloat(TransitionCurve, UpdateDelegate);

		FOnTimelineEvent FinishedDelegate;
		FinishedDelegate.BindUFunction(this, FName("HandleTimelineFinished"));
		CrushTimeline->SetTimelineFinishedFunc(FinishedDelegate);
	}

}

void UkdCrushTransitionComponent::StartTransition(bool bToCrushMode)
{
	if (!CachedOwner || !TransitionCurve) return;

	bTargetCrushMode = bToCrushMode;

	InitialScale = CachedOwner->GetMesh()->GetRelativeScale3D();						// Cache starting values
	TargetScale = bTargetCrushMode ? PlayerCrushScale : FVector(1.0f, 1.0f, 1.0f);		// Determine Target Scale

	InitialOrthoWidth = CachedOwner->Camera->OrthoWidth;
	TargetOrthoWidth = bTargetCrushMode ? OrthoWidth : InitialOrthoWidth;

	CrushTimeline->SetPlayRate(1.0f / TransitionDuration);
	CrushTimeline->PlayFromStart();

}

void UkdCrushTransitionComponent::HandleTimelineUpdate(float Value)
{
	if (!CachedOwner) return;

	// In Update Loop
	float CurveValue = TransitionCurve ? TransitionCurve->GetFloatValue(Value) : Value;

	// Curve value drives interpolation
	FVector NewScale = FMath::Lerp(InitialScale, TargetScale, CurveValue);
	CachedOwner->GetMesh()->SetRelativeScale3D(NewScale);

	float NewOrthoWidth = FMath::Lerp(InitialOrthoWidth, TargetOrthoWidthInternal, CurveValue);
	CachedOwner->Camera->SetOrthoWidth(NewOrthoWidth);

	// Switch projection at midpoint
	if (Value >= 0.5f)
	{
		if (bTargetCrushMode && CachedOwner->Camera->ProjectionMode != ECameraProjectionMode::Orthographic)
		{
			CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
			CachedOwner->Camera->SetOrthoWidth(NewOrthoWidth);
			CachedOwner->Camera->bAutoCalculateOrthoPlanes = false;
			CachedOwner->SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
			CachedOwner->SpringArm->bInheritYaw = false;
		}
		else if (!bTargetCrushMode && CachedOwner->Camera->ProjectionMode != ECameraProjectionMode::Perspective)
		{
			CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
			CachedOwner->SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));
			CachedOwner->SpringArm->bInheritYaw = true;
		}
	}
}

void UkdCrushTransitionComponent::HandleTimelineFinished()
{
	if (OnTransitionComplete.IsBound())
	{
		OnTransitionComplete.Broadcast(bTargetCrushMode);
	}
}
