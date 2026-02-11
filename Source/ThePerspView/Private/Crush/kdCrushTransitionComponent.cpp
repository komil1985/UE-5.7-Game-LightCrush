// Copyright ASKD Games


#include "Crush/kdCrushTransitionComponent.h"
#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UkdCrushTransitionComponent::UkdCrushTransitionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	TimerInterval = 0.016f;	// High refresh rate for smooth animation
}

void UkdCrushTransitionComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedOwner = CastChecked<AkdMyPlayer>(GetOwner());
}

void UkdCrushTransitionComponent::StartTransition(bool bToCrushMode)
{
	if (!CachedOwner) return;

	bTargetCrushMode = bToCrushMode;
	CurrentAlpha = 0.0f;

	
	InitialScale = CachedOwner->GetMesh()->GetRelativeScale3D();						// Cache starting values
	TargetScale = bTargetCrushMode ? PlayerCrushScale : FVector(1.0f, 1.0f, 1.0f);		// Determine Target Scale

	InitialOrthoWidth = CachedOwner->Camera->OrthoWidth;
	TargetOrthoWidth = bTargetCrushMode ? OrthoWidth : InitialOrthoWidth;

	// Start the Loop
	GetWorld()->GetTimerManager().SetTimer(TransitionTimerHandle, this, &UkdCrushTransitionComponent::HandleTransitionUpdate, TimerInterval, true);
}

void UkdCrushTransitionComponent::HandleTransitionUpdate()
{
	if (!CachedOwner)
	{
		GetWorld()->GetTimerManager().ClearTimer(TransitionTimerHandle);
		return;
	}

	// 1. Advance Alpha
	CurrentAlpha += TimerInterval / TransitionDuration;
	CurrentAlpha = FMath::Clamp(CurrentAlpha, 0.0f, 1.0f);

	// 2. Interpolate Mesh Scale
	FVector NewScale = FMath::Lerp(InitialScale, TargetScale, CurrentAlpha);
	CachedOwner->GetMesh()->SetRelativeScale3D(NewScale);

	float NewOrthoWidth = FMath::Lerp(InitialOrthoWidth, TargetOrthoWidth, CurrentAlpha);
	CachedOwner->Camera->SetOrthoWidth(NewOrthoWidth);

	// 3. Interpolate Camera (Optional: Smooth Ortho Transition)
	// Note: Switching ProjectionMode is instant, so we usually do it at 50%
	if (CurrentAlpha >= 0.5f)
	{
		if (bTargetCrushMode && CachedOwner->Camera->ProjectionMode != ECameraProjectionMode::Orthographic)
		{
			CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
			CachedOwner->Camera->SetOrthoWidth(OrthoWidth);
			CachedOwner->Camera->bAutoCalculateOrthoPlanes = false;
			CachedOwner->SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
		}
		//else if (!bTargetCrushMode && CachedOwner->Camera->ProjectionMode != ECameraProjectionMode::Perspective)
		else
		{
			CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
			CachedOwner->SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));
		}
		FinishTransition();
	}

	// 4. Check for Finish
	//if (CurrentAlpha >= 1.0f)
	//{
	//	if (bTargetCrushMode)
	//	{
	//		CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	//	}
	//	else
	//	{
	//		CachedOwner->Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
	//	}
	//	FinishTransition();
	//}
}

void UkdCrushTransitionComponent::FinishTransition()
{
	GetWorld()->GetTimerManager().ClearTimer(TransitionTimerHandle);

	if (OnTransitionComplete.IsBound())
	{
		OnTransitionComplete.Broadcast(bTargetCrushMode);
	}
}
