// Copyright ASKD Games


#include "Crush/kdCrushTransitionComponent.h"
#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UkdCrushTransitionComponent::UkdCrushTransitionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	TransitionDuration = 0.3f;
	TimerInterval = 0.01f;	// High refresh rate for smooth animation
}

void UkdCrushTransitionComponent::BeginPlay()
{
	Super::BeginPlay();

	Owner = CastChecked<AkdMyPlayer>(GetOwner());
}

void UkdCrushTransitionComponent::StartTransition(bool bToCrushMode)
{
	if (!Owner) return;

	bTargetCrushMode = bToCrushMode;
	CurrentAlpha = 0.0f;

	
	InitialScale = Owner->GetMesh()->GetRelativeScale3D();								// Cache starting values
	TargetScale = bTargetCrushMode ? PlayerCrushScale : FVector(1.0f, 1.0f, 1.0f);		// Determine Target Scale

	InitialOrthoWidth = Owner->Camera->OrthoWidth;
	TargetOrthoWidth = bTargetCrushMode ? OrthoWidth : InitialOrthoWidth;

	// Start the Loop
	GetWorld()->GetTimerManager().SetTimer(TransitionTimerHandle, this, &UkdCrushTransitionComponent::HandleTransitionUpdate, TimerInterval, true);
}

void UkdCrushTransitionComponent::HandleTransitionUpdate()
{
	if (!Owner)
	{
		GetWorld()->GetTimerManager().ClearTimer(TransitionTimerHandle);
		return;
	}

	// 1. Advance Alpha
	CurrentAlpha += TimerInterval / TransitionDuration;
	CurrentAlpha = FMath::Clamp(CurrentAlpha, 0.0f, 1.0f);

	// 2. Interpolate Mesh Scale
	FVector NewScale = FMath::Lerp(InitialScale, TargetScale, CurrentAlpha);
	Owner->GetMesh()->SetRelativeScale3D(NewScale);

	float NewOrthoWidth = FMath::Lerp(InitialOrthoWidth, TargetOrthoWidth, CurrentAlpha);
	Owner->Camera->SetOrthoWidth(NewOrthoWidth);

	//// 3. Interpolate Camera (Optional: Smooth Ortho Transition)
	//// Note: Switching ProjectionMode is instant, so we usually do it at 50%
	//if (CurrentAlpha >= 0.5f)
	//{
	//	if (bTargetCrushMode && Owner->Camera->ProjectionMode != ECameraProjectionMode::Orthographic)
	//	{
	//		Owner->Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	//		Owner->Camera->SetOrthoWidth(OrthoWidth);
	//	}
	//	else if (!bTargetCrushMode && Owner->Camera->ProjectionMode != ECameraProjectionMode::Perspective)
	//	{
	//		Owner->Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
	//	}
	//}

	// 4. Check for Finish
	if (CurrentAlpha >= 1.0f)
	{
		if (bTargetCrushMode)
		{
			Owner->Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
		}
		else
		{
			Owner->Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
		}
		FinishTransition();
	}
}

void UkdCrushTransitionComponent::FinishTransition()
{
	GetWorld()->GetTimerManager().ClearTimer(TransitionTimerHandle);

	if (OnTransitionComplete.IsBound())
	{
		OnTransitionComplete.Broadcast(bTargetCrushMode);
	}
}
