// Copyright ASKD Games


#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AkdMyPlayer::AkdMyPlayer()
{
	PrimaryActorTick.bCanEverTick = false;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 500.0f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = false;
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 1000.0f, 0.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

}

void AkdMyPlayer::ToggleCrushMode()
{
	// Toggle the crush mode state
	bIsCrushMode = !bIsCrushMode;

	if (bIsCrushMode)
	{
		// Setting up for orthographic top-down view
		SpringArm->SetRelativeRotation(FRotator(0.0f, 0.0, 0.0f));
		Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
		Camera->OrthoWidth = 500.0f;
		Camera->bAutoCalculateOrthoPlanes = false;

		// Enable planar movement constraints & Set the constraint plane normal to (1, 0, 0) for the YZ plane (restricts X)
		GetCharacterMovement()->bConstrainToPlane = true;
		GetCharacterMovement()->SetPlaneConstraintNormal(FVector(1.0f, 0.0f, 0.0f));
	}
	else
	{
		// Setting up for perspective third-person view
		SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0, 0.0f));
		SpringArm->TargetArmLength = 500.0f;
		Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
		GetCharacterMovement()->bConstrainToPlane = false;
	}
}

void AkdMyPlayer::BeginPlay()
{
	Super::BeginPlay();
	
}

void AkdMyPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}