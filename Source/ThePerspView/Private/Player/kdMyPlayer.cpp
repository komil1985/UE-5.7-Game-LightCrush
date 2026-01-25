// Copyright ASKD Games


#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "World/kdFloorBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

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

void AkdMyPlayer::BeginPlay()
{
	Super::BeginPlay();
	
	// Initialize FloorActor reference
	FloorActor = FindFloorActor(GetWorld());
}

void AkdMyPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

AkdFloorBase* AkdMyPlayer::FindFloorActor(UWorld* World)
{
	// Find the first instance of AkdFloorBase in the world
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AkdFloorBase::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		// Return the first found actor casted to AkdFloorBase
		return Cast<AkdFloorBase>(FoundActors[0]);
	}
	return nullptr;
}

void AkdMyPlayer::ToggleCrushMode()
{
	// Toggle the crush mode state
	bIsCrushMode = !bIsCrushMode;

	if (bIsCrushMode)
	{

		// Setting up for orthographic view
		SpringArm->SetRelativeRotation(FRotator(0.0f, 0.0, 0.0f));
		Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
		Camera->OrthoWidth = 500.0f;
		Camera->bAutoCalculateOrthoPlanes = false;

		// Enable planar movement constraints & Set the constraint plane normal to (1, 0, 0) for the YZ plane (restricts X)
		GetCharacterMovement()->bConstrainToPlane = true;
		GetCharacterMovement()->SetPlaneConstraintNormal(FVector(1.0f, 0.0f, 0.0f));

		// reduce floor X scale to simulate crush effect
		if (FloorActor)
		{
			// Crush the floor along the X axis
			FVector FloorScale = FloorActor->FloorMesh->GetComponentScale();
			FloorScale.X = 1.0f; // Adjust this value to control the degree of "crush"
			FloorActor->FloorMesh->SetRelativeScale3D(FloorScale);

			// Adjust player position to be above the crushed floor
			FVector PlayerLocation = GetActorLocation();
			PlayerLocation.X = 0.0f; // Keep player at X = 0 in crush mode
			SetActorLocation(PlayerLocation);
		}
	}
	else
	{
		// Setting up for perspective third-person view
		SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0, 0.0f));
		SpringArm->TargetArmLength = 500.0f;
		Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
		GetCharacterMovement()->bConstrainToPlane = false;

		// Restore floor scale and player position
		if (FloorActor)
		{
			// Restore the floor scale along the X axis
			FVector FloorScale = FloorActor->FloorMesh->GetComponentScale();
			FloorScale.X = 20.0f; // Restore original scale
			FloorActor->FloorMesh->SetRelativeScale3D(FloorScale);

			// Adjust player position back to normal
			FVector PlayerLocation = GetActorLocation();
			PlayerLocation.X = 0.0f; // Move player back to original X position
			SetActorLocation(PlayerLocation);
		}
	}
}