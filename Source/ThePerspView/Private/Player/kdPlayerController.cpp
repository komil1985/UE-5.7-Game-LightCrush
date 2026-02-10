// Copyright ASKD Games


#include "Player/kdPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "Player/kdMyPlayer.h"

void AkdPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	EnhancedSubSystem();
}

void AkdPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AkdPlayerController::Move);
		}
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AkdPlayerController::Look);
		}
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AkdPlayerController::StartJump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AkdPlayerController::StopJump);
		}
		if (CrushAction)
		{
			EnhancedInputComponent->BindAction(CrushAction, ETriggerEvent::Started, this, &AkdPlayerController::RequestCrushToggle);
		}
		if (MoveUpAction)
		{
			EnhancedInputComponent->BindAction(MoveUpAction, ETriggerEvent::Triggered, this, &AkdPlayerController::HandleCrushMovement);
		}
	}
}

void AkdPlayerController::Move(const FInputActionValue& InputActionValue)
{
	if (AkdMyPlayer* MyPlayer = GetMyPlayer())
	{
		if (!MyPlayer->bIsCrushMode)
		{
			const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
			const FRotator Rotation = GetControlRotation();
			const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);

			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			MyPlayer->AddMovementInput(ForwardDirection, InputAxisVector.Y);
			MyPlayer->AddMovementInput(RightDirection, InputAxisVector.X);
		}
	}
}

void AkdPlayerController::Look(const FInputActionValue& Value)
{
	const FVector2D Vector2D = Value.Get<FVector2D>();
	AddYawInput(Vector2D.X);
	AddPitchInput(Vector2D.Y);
}

void AkdPlayerController::StartJump()
{
	if (AkdMyPlayer* MyPlayer = GetMyPlayer())
	{
		if (!MyPlayer->bIsCrushMode)
		{
			MyPlayer->Jump();
		}
	}
}

void AkdPlayerController::StopJump()
{
	if (AkdMyPlayer* MyPlayer = GetMyPlayer())
	{
		MyPlayer->StopJumping();
	}
}

void AkdPlayerController::EnhancedSubSystem()
{
	if (UEnhancedInputLocalPlayerSubsystem* SubSystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (InputMappingContext)
		{
			SubSystem->AddMappingContext(InputMappingContext, 0);
		}
	}
}

void AkdPlayerController::RequestCrushToggle()
{
	if (AkdMyPlayer* MyPlayer = GetMyPlayer())
	{
		MyPlayer->RequestCrushToggle();
	}
}

void AkdPlayerController::HandleCrushMovement(const FInputActionValue& Value)
{
	if (AkdMyPlayer* MyPlayer = GetMyPlayer())
	{
		// Only valid in Crush Mode
		if (MyPlayer->bIsCrushMode)
		{
			MyPlayer->RequestVerticalMove(Value);
		}
	}
}

TObjectPtr<AkdMyPlayer> AkdPlayerController::GetMyPlayer() const
{
	return Cast<AkdMyPlayer>(GetPawn());
}
