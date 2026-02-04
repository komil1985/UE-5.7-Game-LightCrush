// Copyright ASKD Games


#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "World/kdFloorBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Sound/SoundBase.h"
#include "Engine/DirectionalLight.h"

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
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 10000.0f, 0.0f);

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

	// Store original player location & Scale
	OriginalPlayerLocation = GetActorLocation();
	OriginalPlayerScale = GetMesh()->GetRelativeScale3D();

	// Initialize current floor actor
	UpdateCurrentFloor();


	if (CrushPostProcessMaterial && Camera)
	{
		// Create the dynamic instance
		CrushPPInstance = UMaterialInstanceDynamic::Create(CrushPostProcessMaterial, this);

		// Add it to the Camera's Post Process Settings
		// 0.0f means it is currently invisible (OFF)
		CrushBlendable.Object = CrushPPInstance;
		CrushBlendable.Weight = 0.0f;

		Camera->PostProcessSettings.WeightedBlendables.Array.Add(CrushBlendable);
	}

}

void AkdMyPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle crush mode transitions
	CrushInterpolation(DeltaTime);

	// Update current floor actor each tick
	//UpdateCurrentFloor();

	// Cache player relative position if floor actor has changed
	//if (CurrentFloorActor && LastCachedFloor != CurrentFloorActor)
	//{
	//	CachePlayerRelativePosition();
	//	LastCachedFloor = CurrentFloorActor;
	//}


	// MOVEMENT LOGIC
	// Only override physics if we are fully in Crush Mode (2D)
	if (bIsCrushMode && !bIsTransitioning)
	{
		if (IsStandingInShadow())
		{
			// CASE: Standing on Shadow
			GetCharacterMovement()->SetMovementMode(MOVE_Flying);			// We disable gravity so the player doesn't fall through the shadow
			GetCharacterMovement()->BrakingDecelerationFlying = 500.0f;		// High deceleration for tight control on shadows
			GetCharacterMovement()->GravityScale = 0.0f;
			GetCharacterMovement()->bConstrainToPlane = false;				// Disable plane constraint to allow free movement on shadow
		}
		else
		{
			// CASE: In Empty Light (Gap)
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);		// If we aren't on ground AND aren't on a shadow, gravity takes over.
			GetCharacterMovement()->GravityScale = 1.0f;
		}
	}
	else if (!bIsCrushMode)
	{
		// Standard 3D behavior
		if (GetCharacterMovement()->MovementMode == MOVE_Flying)
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}
	}
}

void AkdMyPlayer::UpdateCurrentFloor()
{
	if (GetCharacterMovement()->CurrentFloor.HitResult.bBlockingHit) {
		AActor* FloorActor = GetCharacterMovement()->CurrentFloor.HitResult.GetActor();
		// Do your logic here
		CurrentFloorActor = Cast<AkdFloorBase>(FloorActor);
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
	if (bIsTransitioning) return;

	bIsTransitioning = true;
	TransitionAlpha = 0.0f;
	bTargetCrushMode = !bIsCrushMode;
	TransitionDuration = 0.2f;

	// Camera Shake when crush mode activates
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && CrushCameraShake)
	{
		PC->PlayerCameraManager->StartCameraShake(CrushCameraShake, 1.0f);
	}

	// Spawning sound when crush mode activates
	if (ToggleCrushSound)
	{
		UGameplayStatics::SpawnSoundAttached(ToggleCrushSound, GetRootComponent());
	}

	// Cache Start values
	TransitionData.PlayerStartLocation = GetActorLocation();
	TransitionData.PlayerOriginalScale = GetMesh()->GetRelativeScale3D();
	TransitionData.SpringArmStartRotation = SpringArm->GetRelativeRotation();
	TransitionData.SpringArmStartLength = SpringArm->TargetArmLength;

	TransitionData.FloorStartScales.Empty();
	TransitionData.FloorTargetScales.Empty();
	TransitionData.FloorStartLocations.Empty();
	TransitionData.FloorTargetLocations.Empty();

	for (AkdFloorBase* Floor : FloorActors)
	{
		if (Floor && Floor->FloorMesh)
		{
			// Start scale and location
			TransitionData.FloorStartScales.Add(Floor->FloorMesh->GetComponentScale());
			TransitionData.FloorStartLocations.Add(Floor->GetActorLocation());

			// Target scale and location for crush mode
			FVector TargetScale = Floor->OriginalFloorScale;
			FVector TargetLocation = Floor->OriginalFloorLocation;

			if (bTargetCrushMode && CrushSound)
			{
				TargetLocation.X = 0.0f; // Keep floor at X = 0 in crush mode
				TargetScale.X = FloorCrushScale; // Crush effect
			}
			TransitionData.FloorTargetScales.Add(TargetScale);
			TransitionData.FloorTargetLocations.Add(TargetLocation);
			UGameplayStatics::SpawnSoundAttached(CrushSound, GetRootComponent());
		}
	}

	// Player targeted position & Scale
	FVector TargetPlayerLocation = TransitionData.PlayerStartLocation;
	FVector TargetPlayerScale = TransitionData.PlayerOriginalScale;
	if (bTargetCrushMode)
	{
		 TargetPlayerLocation.X = 0.0f; // Keep player at X = 0 only
		 TargetPlayerScale.Y = PlayerCrushScale; // Crush effect

		 // Add 5 units of height to clear the floor collision immediately
		 AddActorWorldOffset(FVector(0, 0, 5.0f));
	}
	TransitionData.PlayerTargetLocation = TargetPlayerLocation;
	TransitionData.PlayerTargetScale = TargetPlayerScale;

	// SpringArm target rotation and length
	if (bTargetCrushMode)
	{
		TransitionData.SpringArmTargetRotation = FRotator(0.0f, 0.0, 0.0f);
		TransitionData.SpringArmTargetLength = 300.0f; // Keep same length
	}
	else
	{
		TransitionData.SpringArmTargetRotation = FRotator(-30.0f, 0.0, 0.0f);
		TransitionData.SpringArmTargetLength = 600.0f;
	}
}

void AkdMyPlayer::CrushInterpolation(float DeltaTime)
{
	if (!bIsTransitioning) return;
	
	TransitionAlpha += DeltaTime / TransitionDuration;
	float Alpha = FMath::Clamp(TransitionAlpha, 0.0f, 1.0f);

	// Interpolate player location
	FVector NewPlayerLocation = FMath::Lerp(TransitionData.PlayerStartLocation, TransitionData.PlayerTargetLocation, Alpha);
	
	// only interpolate the X axis to "Snap" the player to the 2D plane
	if (bIsCrushMode)
	{
		FVector CurrentLoc = GetActorLocation();
		CurrentLoc.X = FMath::Lerp(TransitionData.PlayerStartLocation.X, 0.0f, Alpha);
		SetActorLocation(CurrentLoc);
	}

		// Interpolate SpringArm rotation and length
		FRotator NewSpringArmRotation = FMath::Lerp(TransitionData.SpringArmStartRotation, TransitionData.SpringArmTargetRotation, Alpha);
		SpringArm->SetRelativeRotation(NewSpringArmRotation);
		float NewSpringArmLength = FMath::Lerp(TransitionData.SpringArmStartLength, TransitionData.SpringArmTargetLength, Alpha);
		SpringArm->TargetArmLength = NewSpringArmLength;

		/*************************************************************************************************************************************/
		// Interpolate floor scales and locations
		//for (int32 i = 0; i < FloorActors.Num(); ++i)
		//{
		//	AkdFloorBase* Floor = FloorActors[i];  // Assuming FloorActors and TransitionData arrays are aligned
		//	if (Floor && Floor->FloorMesh)
		//	{
		//		FVector NewFloorScale = FMath::Lerp(TransitionData.FloorStartScales[i], TransitionData.FloorTargetScales[i], Alpha);
		//		Floor->FloorMesh->SetRelativeScale3D(NewFloorScale);

		//		FVector NewFloorLocation = FMath::Lerp(TransitionData.FloorStartLocations[i], TransitionData.FloorTargetLocations[i], Alpha);
		//		Floor->SetActorLocation(NewFloorLocation);
		//	}
		//}
		/*************************************************************************************************************************************/

		// Switch camera projection mode at halfway point
		if (Alpha > 0.5f && !bProjectionSwitched)
		{
			if (bTargetCrushMode)
			{
				Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
				Camera->SetOrthoWidth(TransitionData.SpringArmTargetLength * 2.0f);	
				Camera->bAutoCalculateOrthoPlanes = false;
			}
			else
			{
				Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
			}
			bProjectionSwitched = true;
		}

		// Check if transition is complete
		if (Alpha >= 1.0f)
		{
			bIsTransitioning = false;
			bIsCrushMode = bTargetCrushMode;
			GetCharacterMovement()->bConstrainToPlane = bIsCrushMode;

			if (bIsCrushMode)
			{
				GetCharacterMovement()->SetPlaneConstraintNormal(FVector(1.0f, 0.0f, 0.0f));
				GetCharacterMovement()->SetPlaneConstraintEnabled(true);
			}
			else 
			{
				GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				// GetCharacterMovement()->SetPlaneConstraintEnabled(false);
			}
			bProjectionSwitched = false;
		}

		// ---------------------------------------------------
		// VISUAL UPDATE: Fade the Outline Shader
		// ---------------------------------------------------
		if (Camera->PostProcessSettings.WeightedBlendables.Array.Num() > 0)
		{
			float TargetWeight = bTargetCrushMode ? 1.0f : 0.0f;
			float StartWeight = bTargetCrushMode ? 0.0f : 1.0f;

			// Smoothly interpolate the weight
			float NewWeight = FMath::Lerp(StartWeight, TargetWeight, Alpha);

			// Update the FIRST blendable in the array (our outline material)
			Camera->PostProcessSettings.WeightedBlendables.Array[0].Weight = NewWeight;
		}
}

bool AkdMyPlayer::IsStandingInShadow()
{
	UWorld* World = GetWorld();
	if(!World)	return false;

	// Find the Light Source Actor in the world (assuming there's only one for simplicity)
	if (!DirectionalLightActor)
	{
		TArray<AActor*> FoundLights;
		UGameplayStatics::GetAllActorsOfClass(World, ADirectionalLight::StaticClass(), FoundLights);
		if (FoundLights.Num() > 0)
		{
			DirectionalLightActor = Cast<ADirectionalLight>(FoundLights[0]);
			CachedLightDirection = -DirectionalLightActor->GetActorForwardVector(); // Light direction is opposite to forward vector
		}
		else
		{
			CachedLightDirection = FVector(0, 0, 1); // Default to upward light if no directional light found
		}
	}

	// Perform a line trace from the player to the light source direction
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this); // Ignore self

	FVector Start = GetActorLocation() - FVector(0, 0, 40.0f);
	FVector End = Start + (CachedLightDirection * 50000.0f); // Cast ray towards light source

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);

	if (bShowShadowDebugLines)
	{
		if (bHit)
		{
			DrawDebugLine(World, Start, HitResult.ImpactPoint, FColor::Green, false, -1.0f, 0, 2.0f);
		}
		else
		{
			DrawDebugLine(World, Start, Start + (CachedLightDirection * 1000.0f), FColor::Red, false, -1.0f, 0, 2.0f);
		}
	}
	return bHit;	// If we hit something, player is in shadow; if not, player is in light
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
	// Start the transition
	CrushTransition();		// <-- Use this transition function instead of instant toggle

}

void AkdMyPlayer::MoveUpInShadow(float Value)
{
	// Safety check: Only move if in Crush mode and in shadow
	if (!bIsCrushMode || bIsTransitioning) return;

	// Only apply vertical movement when in flying mode (on shadow)
	if (GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		float FloatAcceleration = 1500.0f; // How fast we gain speed
		float MaxFloatSpeed = 600.0f;      // Cap the speed

		FVector CurrentVel = GetCharacterMovement()->Velocity;

		// Apply acceleration to the Z axis
		CurrentVel.Z += Value * FloatAcceleration * GetWorld()->GetDeltaSeconds();

		// Clamp the speed so you don't rocket off into space
		CurrentVel.Z = FMath::Clamp(CurrentVel.Z, -MaxFloatSpeed, MaxFloatSpeed);

		GetCharacterMovement()->Velocity = CurrentVel;
	}
}



/****************************************************************************************************************************************************************************/

// -- Previous instant toggle implementation in ToggleCrushMode function --- Below is for reference only -- //

/****************************************************************************************************************************************************************************/

// Toggle the crush mode state
//bIsCrushMode = !bIsCrushMode;

//if (bIsCrushMode)
//{
//	// Cache player position before entering crush mode
//	//CachePlayerRelativePosition();

//	// Setting up for orthographic view
//	SpringArm->SetRelativeRotation(FRotator(0.0f, 0.0, 0.0f));
//	Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
//	Camera->OrthoWidth = 500.0f;
//	Camera->bAutoCalculateOrthoPlanes = false;

//	// Enable planar movement constraints & Set the constraint plane normal to (1, 0, 0) for the YZ plane (restricts X movement)
//	GetCharacterMovement()->bConstrainToPlane = true;
//	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(1.0f, 0.0f, 0.0f));

//	for (AkdFloorBase* Floor : FloorActors)
//	{
//		// reduce floor X scale and position to simulate crush effect
//		if (Floor && Floor->FloorMesh)
//		{
//			// Crush the floor along the X axis
//			FVector NewFloorScale = Floor->OriginalFloorScale;	// Cached original scale
//			NewFloorScale.X = 1.0f; // Adjust this value to control the degree of "crush"
//			Floor->FloorMesh->SetRelativeScale3D(NewFloorScale);
//			
//			// Adjust Floor position
//			FVector NewFloorLocation = Floor->OriginalFloorLocation;	// Cached original location
//			NewFloorLocation.X = 0.0f; // Keep floor at X = 0 in crush mode
//			Floor->SetActorLocation(NewFloorLocation);
//		}
//	}

//	if (CurrentFloorActor && PlayerRelativePositionsPerFloor.Contains(CurrentFloorActor))
//	{
//		// Adjust player position to be above the crushed floor
//		FVector PlayerRelativePosition = PlayerRelativePositionsPerFloor[CurrentFloorActor];
//		PlayerRelativePosition.X = 0.0f; // Keep player at X = 0 in crush mode
//		FVector NewPlayerLocation = CurrentFloorActor->GetActorLocation() + PlayerRelativePosition;
//		SetActorLocation(NewPlayerLocation);
//	}
//	else
//	{
//		// If no current floor, just set player X to 0
//		FVector PlayerLocation = GetActorLocation();
//		PlayerLocation.X = 0.0f;
//		SetActorLocation(PlayerLocation);
//	}
//}
//else  // Restore Mode
//{
//	// Setting up for perspective third-person view
//	SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0, 0.0f));
//	SpringArm->TargetArmLength = 500.0f;
//	Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
//	
//	GetCharacterMovement()->bConstrainToPlane = false;

//	// Restore floor scale and position
//	for (AkdFloorBase* Floor : FloorActors)
//	{
//		if (Floor && Floor->FloorMesh)
//		{
//			// Restore the floor scale
//			Floor->FloorMesh->SetRelativeScale3D(Floor->OriginalFloorScale);

//			// Restore Floor position
//			Floor->SetActorLocation(Floor->OriginalFloorLocation);
//		}
//	}
//}

/****************************************************************************************************************************************************************************/

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/****************************************************************************************************************************************************************************/