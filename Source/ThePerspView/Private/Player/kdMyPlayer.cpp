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
	
	// Initialize FloorActors reference
	FindFloorActors(GetWorld());
}

void AkdMyPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

AkdFloorBase* AkdMyPlayer::FindFloorActors(UWorld* World)
{
	// Find the first instance of AkdFloorBase in the world
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AkdFloorBase::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		if (AkdFloorBase* Floor = Cast<AkdFloorBase>(Actor))
		{
			FloorActors.Add(Floor);
		}
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

		// Enable planar movement constraints & Set the constraint plane normal to (1, 0, 0) for the YZ plane (restricts X movement)
		GetCharacterMovement()->bConstrainToPlane = true;
		GetCharacterMovement()->SetPlaneConstraintNormal(FVector(1.0f, 0.0f, 0.0f));

		for (AkdFloorBase* Floor : FloorActors)
		{
			// reduce floor X scale and position to simulate crush effect
			if (Floor && Floor->FloorMesh)
			{
				// Crush the floor along the X axis
				FVector FloorScale = Floor->FloorMesh->GetComponentScale();
				FloorScale.X = 1.0f; // Adjust this value to control the degree of "crush"
				Floor->FloorMesh->SetRelativeScale3D(FloorScale);

				// Adjust Floor position
				FVector FloorLocation = Floor->GetActorLocation();
				FloorLocation.X = 0.0f; // Keep floor at X = 0 in crush mode
				Floor->SetActorLocation(FloorLocation);

				// Adjust player position to be above the crushed floor
				FVector PlayerLocation = GetActorLocation();
				PlayerLocation.X = 0.0f; // Keep player at X = 0 in crush mode
				SetActorLocation(PlayerLocation);
			}
			else
			{
				// Setting up for perspective third-person view
				SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0, 0.0f));
				SpringArm->TargetArmLength = 500.0f;
				Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
				GetCharacterMovement()->bConstrainToPlane = false;

				// Restore floor scale and player position
				if (Floor && Floor->FloorMesh)
				{
					// Restore the floor scale along the X axis
					FVector FloorScale = Floor->FloorMesh->GetComponentScale();
					FloorScale.X = 20.0f; // Restore original scale
					Floor->FloorMesh->SetRelativeScale3D(FloorScale);

					// Restore Floor position
					FVector FloorLocation = Floor->GetActorLocation();
					FloorLocation = Floor->GetFloorLocation(); // Move floor back to original X position
					Floor->SetActorLocation(FloorLocation);

					// Adjust player position back to normal
					FVector PlayerLocation = GetActorLocation();
					PlayerLocation.X = 0.0f; // Move player back to original X position
					SetActorLocation(PlayerLocation);
				}
			}
		}
	}
}