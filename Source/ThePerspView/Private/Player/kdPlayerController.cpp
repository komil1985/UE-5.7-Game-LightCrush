// Copyright ASKD Games


#include "Player/kdPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "Player/kdMyPlayer.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"


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
		ASC = MyPlayerCache->GetAbilitySystemComponent();
	}
}

void AkdPlayerController::Move(const FInputActionValue& InputActionValue)
{
	if (!MyPlayerCache) return;
	
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	
	if (!ASC || !ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
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
	if (!MyPlayer || !ASC || ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode)) return;			// Do not process look input in 2D mode (Crush mode)
	
	const FVector2D LookAxisValue = Value.Get<FVector2D>();
	AddYawInput(LookAxisValue.X);
	AddPitchInput(-LookAxisValue.Y);	
}

void AkdPlayerController::StartJump()
{
	if (MyPlayerCache)
	{
		if (!ASC || !ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
		{
			MyPlayerCache->Jump();
		}
	}
}

void AkdPlayerController::StopJump()
{
	if (MyPlayerCache)
	{
		if (!ASC || !ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
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

void AkdPlayerController::RequestCrushToggle()
{
	if (MyPlayerCache)
	{
		MyPlayerCache->RequestCrushToggle();
	}
}

void AkdPlayerController::HandleShadowMovement(const FInputActionValue& Value)
{
	if (MyPlayerCache)
	{
		// Only valid in Crush Mode
		if (ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
		{
			MyPlayerCache->RequestVerticalMove(Value);
#if !UE_BUILD_SHIPPING
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.0f, FColor::Green, TEXT("Launch Activated"));
#endif
		}
	}
}

TObjectPtr<AkdMyPlayer> AkdPlayerController::GetMyPlayer() const
{
	return Cast<AkdMyPlayer>(GetPawn());
}
