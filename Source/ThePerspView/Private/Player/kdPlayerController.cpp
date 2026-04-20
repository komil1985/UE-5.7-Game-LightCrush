// Copyright ASKD Games


#include "Player/kdPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/kdShadowMove.h"
#include "UI/HUD/kdHUD.h"
#include "GameMode/kdGameModeBase.h"
#include "UI/Widget/kdPauseMenuWidget.h"


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
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started,
				this, &AkdPlayerController::Interact);
		}
		if (DashAction)
		{
			EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started,
				this, &AkdPlayerController::ShadowDash);
		}
		if (DebugPrintTagsAction)
		{
			EnhancedInputComponent->BindAction(DebugPrintTagsAction, ETriggerEvent::Started, this, &AkdPlayerController::PrintTags);
		}
		if (PauseAction)
		{
			EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Started, this, &AkdPlayerController::TogglePause);
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

	// FInputModeGameOnly routes all mouse movement directly to the game.
	// Camera rotates freely with no click required — matching original behaviour.
	// UI widgets (stamina bar, HUD overlays) remain visible; they just don't
	// consume mouse input during gameplay, which is correct.
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
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
		MyPlayerCache->AddMovementInput(FVector(0.0f, 1.0f, 0.0f), InputAxisVector.X);
		MyPlayerCache->AddMovementInput(FVector(0.0f, 0.0f, 1.0f), InputAxisVector.Y);
	}
	
}

void AkdPlayerController::Look(const FInputActionValue& Value)
{
	AkdMyPlayer* MyPlayer = MyPlayerCache;
	if (!MyPlayer || !MyASC || MyASC->HasMatchingGameplayTag(StateTags.State_CrushMode)) return;			// look input is not processed in 2D mode (Crush mode)
	
	const FVector2D LookAxisValue = Value.Get<FVector2D>();
	AddYawInput(LookAxisValue.X);
	AddPitchInput(-LookAxisValue.Y);	
}

void AkdPlayerController::StartJump()
{
	if (!MyPlayerCache) return;

	// Block jump entirely in Crush Mode (covers both crush-wandering and in-shadow states)
    // State_InShadow is always a sub-state of State_CrushMode, so one check is sufficient
	const bool bJumpBlocked = MyASC && MyASC->HasMatchingGameplayTag(StateTags.State_CrushMode);
	if (!bJumpBlocked)
	{
		MyPlayerCache->Jump();
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

void AkdPlayerController::Interact()
{
	if (MyPlayerCache) MyPlayerCache->RequestInteract();
}

void AkdPlayerController::ShadowDash()
{
	if (MyPlayerCache) MyPlayerCache->RequestShadowDash();
}

void AkdPlayerController::TogglePause()
{
	AkdHUD* HUD = Cast<AkdHUD>(GetHUD());
	if (!HUD) return;

	if (IsPaused())
	{
		HUD->HidePauseMenu();
	}
	else
	{
		if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
		{
			if (!GM->IsGamePaused())
			{
				if (UkdPauseMenuWidget* W = HUD->GetPauseMenu())
				{
					W->RefreshLivesDisplay(GM->GetRemainingLives());
					HUD->ShowPauseMenu();
				}
			}
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

void AkdPlayerController::PrintTags()
{
	if (MyPlayerCache && MyPlayerCache->GetAbilitySystemComponent())
	{
		FGameplayTagContainer TagsContainer;
		MyPlayerCache->GetAbilitySystemComponent()->GetOwnedGameplayTags(TagsContainer);
		UE_LOG(LogTemp, Log, TEXT("Current tags: %s"), *TagsContainer.ToStringSimple());
	}
	
}

