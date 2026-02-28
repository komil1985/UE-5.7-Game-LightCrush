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
	UE_LOG(LogTemp, Log, TEXT("StartTransition called with bToCrushMode=%d"), bToCrushMode);
	//if (!CachedOwner || !TransitionCurve) return;

	if (!CachedOwner)
	{
		UE_LOG(LogTemp, Error, TEXT("CachedOwner is null"));
		return;
	}
	if (!TransitionCurve)
	{
		UE_LOG(LogTemp, Error, TEXT("TransitionCurve is null. Assign a curve in the component defaults "));
		return;
	}

	bTargetCrushMode = bToCrushMode;

	InitialScale = CachedOwner->GetMesh()->GetRelativeScale3D();							// Cache starting values
	TargetScaleCache = bTargetCrushMode ? PlayerCrushScale : FVector(1.0f, 1.0f, 1.0f);		// Determine Target Scale

	InitialOrthoWidth = CachedOwner->Camera->OrthoWidth;
	TargetOrthoWidthCache = bTargetCrushMode ? OrthoWidth : InitialOrthoWidth;

	// Set projection mode early to avoid pop (or blend later)
	if (bTargetCrushMode && CachedOwner->Camera->ProjectionMode != ECameraProjectionMode::Orthographic)
	{
		CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
		CachedOwner->Camera->SetOrthoWidth(TargetOrthoWidthCache);
		CachedOwner->Camera->bAutoCalculateOrthoPlanes = false;
	}
	else if (!bTargetCrushMode && CachedOwner->Camera->ProjectionMode != ECameraProjectionMode::Perspective)
	{
		CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
	}

	CrushTimeline->SetPlayRate(1.0f / TransitionDuration);
	CrushTimeline->PlayFromStart();

}

void UkdCrushTransitionComponent::HandleTimelineUpdate(float Value)
{
	UE_LOG(LogTemp, Verbose, TEXT("Timeline update: %f"), Value); // Verbose to avoid spam

	if (!CachedOwner) return;

	// In Update Loop
	float CurveValue = TransitionCurve ? TransitionCurve->GetFloatValue(Value) : Value;

	// Curve value drives interpolation
	FVector NewScale = FMath::Lerp(InitialScale, TargetScaleCache, CurveValue);
	CachedOwner->GetMesh()->SetRelativeScale3D(NewScale);

	float NewOrthoWidth = FMath::Lerp(InitialOrthoWidth, TargetOrthoWidthCache, CurveValue);
	CachedOwner->Camera->SetOrthoWidth(NewOrthoWidth);

	// Optionally adjust spring arm rotation based on mode
	if (bTargetCrushMode)
	{
		CachedOwner->SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
		CachedOwner->SpringArm->bInheritYaw = false;
	}
	else
	{
		CachedOwner->SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));
		CachedOwner->SpringArm->bInheritYaw = true;
	}

	// Switch projection at midpoint
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
}

void UkdCrushTransitionComponent::HandleTimelineFinished()
{
	if (OnTransitionComplete.IsBound())
	{
		UE_LOG(LogTemp, Log, TEXT("Timeline finished, broadcasting OnTransitionComplete"));
		OnTransitionComplete.Broadcast(bTargetCrushMode);
	}
}
