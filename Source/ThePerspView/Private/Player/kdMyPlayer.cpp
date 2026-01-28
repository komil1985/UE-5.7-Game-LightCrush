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
	PrimaryActorTick.bCanEverTick = true;

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

	CurrentFloorActor = nullptr;
}

void AkdMyPlayer::BeginPlay()
{
	Super::BeginPlay();

	// Initialize FloorActors reference
	FindFloorActors(GetWorld());

	// Store original player location
	OriginalPlayerLocation = GetActorLocation();

	// Initialize current floor actor
	UpdateCurrentFloor();

}

void AkdMyPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update current floor actor each tick
	UpdateCurrentFloor();

	// Handle crush mode transitions
	CrushInterpolation(DeltaTime); 

}

void AkdMyPlayer::UpdateCurrentFloor()
{
	// Perform a line trace downwards to detect the floor actor beneath the player
	FVector Start = GetActorLocation();
	FVector End = Start - FVector(0.0f, 0.0f, 200.0f); // Cast ray downwards

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// Line trace to find the floor actor
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);

	if(bHit)
	{	
		// Check if the hit actor is a floor actor
		if(AkdFloorBase* HitFloor = Cast<AkdFloorBase>(HitResult.GetActor()))
		{
			CurrentFloorActor = HitFloor;
		}
	}
	else
	{
		CurrentFloorActor = nullptr;  // No floor actor found
	}
}

void AkdMyPlayer::CachePlayerRelativePosition()
{
	// Cache player position relative to the current floor actor
	if (CurrentFloorActor)
	{
		FVector FloorLocation = CurrentFloorActor->GetActorLocation();
		FVector RelativePosition = GetActorLocation() - FloorLocation;
		PlayerRelativePositionsPerFloor.Add(CurrentFloorActor, RelativePosition);
	}
}

void AkdMyPlayer::CrushTransition()
{
	if (bIsTranstioning) return;

	bIsTranstioning = true;
	TransitionAlpha = 0.0f;
	bTargetCrushMode = !bIsCrushMode;

	// You would typically use a timer or a tick function to update TransitionAlpha over time

	// Cache Start values
	TransitionData.PlayerStartLocation = GetActorLocation();
	TransitionData.SpringArmStartRotation = SpringArm->GetRelativeRotation();
	TransitionData.SpringArmStartLength = SpringArm->TargetArmLength;

	TransitionData.FloorStartScales.Empty();
	TransitionData.FloorTargetScales.Empty();
	TransitionData.FloorStartLocations.Empty();
	TransitionData.FloorTargetLocations.Empty();

	for (AkdFloorBase* Floor : FloorActors)
	{
		if(Floor && Floor->FloorMesh)
		{
			// Start scale and location
			TransitionData.FloorStartScales.Add(Floor->FloorMesh->GetComponentScale());
			TransitionData.FloorStartLocations.Add(Floor->GetActorLocation());

			// Target scale and location for crush mode
			FVector TargetScale = Floor->OriginalFloorScale;
			FVector TargetLocation = Floor->OriginalFloorLocation;

			if (bTargetCrushMode)
			{
				TargetScale.X = 1.0f; // Crush effect
				TargetLocation.X = 0.0f; // Keep floor at X = 0 in crush mode
			}

			TransitionData.FloorTargetScales.Add(TargetScale);
			TransitionData.FloorTargetLocations.Add(TargetLocation);
		}
	}
	// You would typically set up a timer or use Tick to call an UpdateTransition function here

	// Player position target
	FVector TargetPlayerLocation = TransitionData.PlayerStartLocation;
	if (bTargetCrushMode)
	{
		TargetPlayerLocation.X = 0.0f; // Keep player at X = 0 in crush mode
	}
	TransitionData.PlayerTargetLocation = TargetPlayerLocation;

	// SpringArm target rotation and length
	if (bTargetCrushMode)
	{
		TransitionData.SpringArmTargetRotation = FRotator(0.0f, 0.0, 0.0f);
		TransitionData.SpringArmTargetLength = SpringArm->TargetArmLength; // Keep same length
	}
	else
	{
		TransitionData.SpringArmTargetRotation = FRotator(-30.0f, 0.0, 0.0f);
		TransitionData.SpringArmTargetLength = 500.0f;
	}

}

void AkdMyPlayer::CrushInterpolation(float DeltaTime)
{
	if (bIsTranstioning)
	{
		TransitionAlpha += DeltaTime / TransitionDuration;
		float Alpha = FMath::Clamp(TransitionAlpha, 0.0f, 1.0f);

		// Interpolate player location
		FVector NewPlayerLocation = FMath::Lerp(TransitionData.PlayerStartLocation, TransitionData.PlayerTargetLocation, Alpha);
		SetActorLocation(NewPlayerLocation);

		// Interpolate SpringArm rotation and length
		FRotator NewSpringArmRotation = FMath::Lerp(TransitionData.SpringArmStartRotation, TransitionData.SpringArmTargetRotation, Alpha);
		SpringArm->SetRelativeRotation(NewSpringArmRotation);
		float NewSpringArmLength = FMath::Lerp(TransitionData.SpringArmStartLength, TransitionData.SpringArmTargetLength, Alpha);
		SpringArm->TargetArmLength = NewSpringArmLength;

		// Interpolate floor scales and locations
		for (int32 i = 0; i < FloorActors.Num(); ++i)
		{
			AkdFloorBase* Floor = FloorActors[i];  // Assuming FloorActors and TransitionData arrays are aligned
			if (Floor && Floor->FloorMesh)
			{
				FVector NewFloorScale = FMath::Lerp(TransitionData.FloorStartScales[i], TransitionData.FloorTargetScales[i], Alpha);
				Floor->FloorMesh->SetRelativeScale3D(NewFloorScale);

				FVector NewFloorLocation = FMath::Lerp(TransitionData.FloorStartLocations[i], TransitionData.FloorTargetLocations[i], Alpha);
				Floor->SetActorLocation(NewFloorLocation);
			}
		}

		// Switch camera projection mode at halfway point
		if (Alpha > 0.5f)
		{
			Camera->SetProjectionMode(bTargetCrushMode ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective);
		}

		// Check if transition is complete
		if (Alpha >= 1.0f)
		{
			bIsTranstioning = false;
			bIsCrushMode = bTargetCrushMode;
			GetCharacterMovement()->bConstrainToPlane = bIsCrushMode;

			if (bIsCrushMode)
			{
				// Ensure constraint plane normal is set for crush mode
				GetCharacterMovement()->SetPlaneConstraintNormal(FVector(1.0f, 0.0f, 0.0f));
			}
		}
	}
}

void AkdMyPlayer::FindFloorActors(UWorld* World)
{
	// Find the first instance of AkdFloorBase in the world
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AkdFloorBase::StaticClass(), FoundActors);

	FloorActors.Empty();
	for (AActor* Actor : FoundActors)
	{
		if (AkdFloorBase* Floor = Cast<AkdFloorBase>(Actor))
		{
			FloorActors.Add(Floor);
		}
	}
}

void AkdMyPlayer::ToggleCrushMode()
{
	// Toggle the crush mode state
	//bIsCrushMode = !bIsCrushMode;

	// Start the transition
	CrushTransition();		// <-- Use transition function instead of instant toggle

	if (bIsCrushMode)
	{
		// Cache player position before entering crush mode
		CachePlayerRelativePosition();

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
				FVector NewFloorScale = Floor->OriginalFloorScale;	// Cached original scale
				NewFloorScale.X = 1.0f; // Adjust this value to control the degree of "crush"
				Floor->FloorMesh->SetRelativeScale3D(NewFloorScale);
				
				// Adjust Floor position
				FVector NewFloorLocation = Floor->OriginalFloorLocation;	// Cached original location
				NewFloorLocation.X = 0.0f; // Keep floor at X = 0 in crush mode
				Floor->SetActorLocation(NewFloorLocation);
			}
		}

		if (CurrentFloorActor && PlayerRelativePositionsPerFloor.Contains(CurrentFloorActor))
		{
			// Adjust player position to be above the crushed floor
			FVector PlayerRelativePosition = PlayerRelativePositionsPerFloor[CurrentFloorActor];
			PlayerRelativePosition.X = 0.0f; // Keep player at X = 0 in crush mode
			FVector NewPlayerLocation = CurrentFloorActor->GetActorLocation() + PlayerRelativePosition;
			SetActorLocation(NewPlayerLocation);
		}
		else
		{
			// If no current floor, just set player X to 0
			FVector PlayerLocation = GetActorLocation();
			PlayerLocation.X = 0.0f;
			SetActorLocation(PlayerLocation);
		}
	}
	else  // Restore Mode
	{
		// Setting up for perspective third-person view
		SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0, 0.0f));
		SpringArm->TargetArmLength = 500.0f;
		Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
		
		GetCharacterMovement()->bConstrainToPlane = false;

		// Restore floor scale and position
		for (AkdFloorBase* Floor : FloorActors)
		{
			if (Floor && Floor->FloorMesh)
			{
				// Restore the floor scale
				Floor->FloorMesh->SetRelativeScale3D(Floor->OriginalFloorScale);

				// Restore Floor position
				Floor->SetActorLocation(Floor->OriginalFloorLocation);
			}
		}
	}
}