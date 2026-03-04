// Copyright ASKD Games


#include "Player/kdPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/kdShadowMove.h"


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
			EnhancedInputComponent->BindAction(CrushAction, ETriggerEvent::Started, this, &AkdPlayerController::CrushToggleRequest);
		}
		if (MoveInShadowAction)
		{
			EnhancedInputComponent->BindAction(MoveInShadowAction, ETriggerEvent::Triggered, this, &AkdPlayerController::HandleShadowMovement);
		}
	}
}

void AkdPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	// Cache the possessed pawn as AkdMyPlayer for easy access
	MyPlayerCache = Cast<AkdMyPlayer>(InPawn);
	if (MyPlayerCache)
	{
		MyASC = MyPlayerCache->GetAbilitySystemComponent();
	}
}

void AkdPlayerController::Move(const FInputActionValue& InputActionValue)
{
	if (!MyPlayerCache) return;
	
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	
	if (!MyASC || !MyASC->HasMatchingGameplayTag(StateTags.State_CrushMode))
	{
		// Normal 3D Movement
		const FRotator Rotation = GetControlRotation();
		const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		MyPlayerCache->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		MyPlayerCache->AddMovementInput(RightDirection, InputAxisVector.X);
	}
	else
	{
		MyPlayerCache->AddMovementInput(FVector(0.f, 1.f, 0.f), InputAxisVector.X);
	}
	
}

void AkdPlayerController::Look(const FInputActionValue& Value)
{
	AkdMyPlayer* MyPlayer = MyPlayerCache;
	if (!MyPlayer || !MyASC || MyASC->HasMatchingGameplayTag(StateTags.State_CrushMode)) return;			// Do not process look input in 2D mode (Crush mode)
	
	const FVector2D LookAxisValue = Value.Get<FVector2D>();
	AddYawInput(LookAxisValue.X);
	AddPitchInput(-LookAxisValue.Y);	
}

void AkdPlayerController::StartJump()
{
	if (MyPlayerCache)
	{
		if (!MyASC || !MyASC->HasMatchingGameplayTag(StateTags.State_CrushMode))
		{
			MyPlayerCache->Jump();
		}
	}
}

void AkdPlayerController::StopJump()
{
	if (MyPlayerCache)
	{
		if (!MyASC || !MyASC->HasMatchingGameplayTag(StateTags.State_CrushMode))
		{
			MyPlayerCache->StopJumping();
		}
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

void AkdPlayerController::CrushToggleRequest()
{
	if (MyPlayerCache)
	{
		MyPlayerCache->RequestCrushToggle();
	}
}

void AkdPlayerController::HandleShadowMovement()
{
	if (!MyPlayerCache || !MyASC) return;
	
	// Only activate if in crush mode AND in shadow (the ability will also check)
	if (MyASC->HasMatchingGameplayTag(StateTags.State_CrushMode) && MyASC->HasMatchingGameplayTag(StateTags.State_InShadow))
	{
		//MyPlayerCache->RequestVerticalMove();
		MyASC->TryActivateAbilityByClass(UkdShadowMove::StaticClass());
	}
}

TObjectPtr<AkdMyPlayer> AkdPlayerController::GetMyPlayer() const
{
	return Cast<AkdMyPlayer>(GetPawn());
}
