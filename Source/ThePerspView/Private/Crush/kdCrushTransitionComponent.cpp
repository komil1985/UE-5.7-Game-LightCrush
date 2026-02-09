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

void UkdCrushTransitionComponent::StartTransition(bool bToCrushMode)
{
	AkdMyPlayer* Owner = Cast<AkdMyPlayer>(GetOwner());
	if (!Owner) return;

	bTargetCrushMode = bToCrushMode;
	CurrentAlpha = 0.0f;

	// Cache starting values
	InitialScale = Owner->GetMesh()->GetRelativeScale3D();

	// Determine Target Scale
	TargetScale = bTargetCrushMode ? PlayerCrushScale : FVector(1.0f, 1.0f, 1.0f);

	// Start the Loop
	GetWorld()->GetTimerManager().SetTimer(TransitionTimerHandle, this, &UkdCrushTransitionComponent::HandleTransitionUpdate, TimerInterval, true);
}

void UkdCrushTransitionComponent::HandleTransitionUpdate()
{
	AkdMyPlayer* Owner = Cast<AkdMyPlayer>(GetOwner());
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

	// 3. Interpolate Camera (Optional: Smooth Ortho Transition)
	// Note: Switching ProjectionMode is instant, so we usually do it at 50%
	if (CurrentAlpha >= 0.5f)
	{
		if (bTargetCrushMode && Owner->Camera->ProjectionMode != ECameraProjectionMode::Orthographic)
		{
			Owner->Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
			Owner->Camera->SetOrthoWidth(OrthoWidth);
		}
		else if (!bTargetCrushMode && Owner->Camera->ProjectionMode != ECameraProjectionMode::Perspective)
		{
			Owner->Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
		}
	}

	// 4. Check for Finish
	if (CurrentAlpha >= 1.0f)
	{
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
