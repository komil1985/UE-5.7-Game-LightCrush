// Copyright ASKD Games


#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Crush/kdCrushStateComponent.h"
#include "Crush/kdCrushTransitionComponent.h"

AkdMyPlayer::AkdMyPlayer()
{
	PrimaryActorTick.bCanEverTick = false;

	/*	--	Components	--	*/
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 500.0f;
	SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true;
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	CrushStateComponent = CreateDefaultSubobject<UkdCrushStateComponent>(TEXT("CrushState"));
	CrushTransitionComponent = CreateDefaultSubobject<UkdCrushTransitionComponent>(TEXT("CrushTransition"));
	/*-----------------------------------------------------------------------------------------------------------*/

	/*	--	Default Values	--	*/
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	bIsCrushMode = false;
	bIsTransitioning = false;
	/*-----------------------------------------------------------------------------------------------------------*/
}


void AkdMyPlayer::BeginPlay()
{
	Super::BeginPlay();

	// Binding Transition Finished Event
	if (CrushTransitionComponent)	CrushTransitionComponent->OnTransitionComplete.AddDynamic(this, &AkdMyPlayer::OnTransitionFinished);
	
}

void AkdMyPlayer::RequestCrushToggle()
{
	if (bIsTransitioning) return;

	bIsTransitioning = true;
	bool bTargetState = !bIsCrushMode;

	// Start Visuals
	if (CrushTransitionComponent)	CrushTransitionComponent->StartTransition(bTargetState);
	
}

void AkdMyPlayer::OnTransitionFinished(bool bNewCrushState)
{
	bIsCrushMode = bNewCrushState;
	bIsTransitioning = false;

	// Tell the physics engine to start/stop tracking shadows
	if (CrushStateComponent)	CrushStateComponent->ToggleShadowTracking(bIsCrushMode);
	

	// Set Plane constraints for 2D movement
	if (bIsCrushMode)
	{
		GetCharacterMovement()->SetPlaneConstraintNormal(PlaneConstraintNormal);
		GetCharacterMovement()->SetPlaneConstraintEnabled(true);

		// Align Player to (X) axis
		FVector NewLocation = GetActorLocation();
		NewLocation.X = PlaneConstraintXValue;
		SetActorLocation(NewLocation);
	}
	else
	{
		GetCharacterMovement()->SetPlaneConstraintEnabled(false);
	}
}

void AkdMyPlayer::RequestVerticalMove(const FInputActionValue& Value)
{
	if (CrushStateComponent)
	{
		CrushStateComponent->HandleVerticalInput(Value.Get<float>());
	}
}
