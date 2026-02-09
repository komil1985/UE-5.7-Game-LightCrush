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
#include "Crush/kdCrushStateComponent.h"
#include "Crush/kdCrushTransitionComponent.h"

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

	CrushStateComponent = CreateDefaultSubobject<UkdCrushStateComponent>(TEXT("CrushState"));
	CrushTransitionComponent = CreateDefaultSubobject<UkdCrushTransitionComponent>(TEXT("CrushTransition"));

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CurrentFloorActor = nullptr;
}

void AkdMyPlayer::AddShadowVerticalInput(float Value)
{
	if (CurrentState == EPlayerMovementState::Crush2D_InShadow)
	{
		AddMovementInput(FVector::UpVector, Value);
	}
}

void AkdMyPlayer::BeginPlay()
{
	Super::BeginPlay();

	CrushTransitionComponent->OnTransitionComplete.AddDynamic(this, &AkdMyPlayer::OnTransitionFinished);

	//////////////////////////////////////////////////////////////////////////////////////////
	
	// Initialize FloorActors reference
	FindFloorActors(GetWorld());

	// Store original player location & Scale
	OriginalPlayerLocation = GetActorLocation();
	OriginalPlayerScale = GetMesh()->GetRelativeScale3D();

	// Initialize current floor actor
	UpdateCurrentFloor();

	// Initialize post-process material for crush effect
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

void AkdMyPlayer::OnTransitionFinished(bool bNewCrushState)
{
	bIsCrushMode = bNewCrushState;
	bIsTransitioning = false;

	// Tell the physics engine to start/stop tracking shadows
	CrushStateComponent->ToggleShadowTracking(bIsCrushMode);
}

void AkdMyPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//CrushInterpolation(DeltaTime);	// Smoothly interpolate player and floor properties during crush/restore transitions
	//UpdateFSM(DeltaTime);			// Update player movement state machine
	//CrushMode();					// Handle crush mode logic based on current state and conditions
	
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
		TransitionData.SpringArmTargetLength = 500.0f; // Keep same length
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
			//GetCharacterMovement()->bConstrainToPlane = bIsCrushMode;
			if (bIsCrushMode)
			{
				GetCharacterMovement()->SetPlaneConstraintNormal(FVector(1.0f, 0.0f, 0.0f));
				GetCharacterMovement()->SetPlaneConstraintEnabled(true);
			}
			else if (bIsCrushMode && IsStandingInShadow())
			{
				GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				GetCharacterMovement()->SetPlaneConstraintEnabled(true);
				GetCharacterMovement()->GravityScale = 0.0f;
				GetCharacterMovement()->SetPlaneConstraintNormal(FVector(1.0f, 0.0f, 0.0f));
			}
			else
			{
				// FORCE the physics back to normal immediately
				GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				GetCharacterMovement()->GravityScale = 1.0f;
				GetCharacterMovement()->SetPlaneConstraintEnabled(false);
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

void AkdMyPlayer::SetMovementState(EPlayerMovementState NewState)
{
	if (CurrentState == NewState) return;	// No state change
	PreviousState = CurrentState;			// Store previous state for reference
	OnExitState(PreviousState);				// Handle any cleanup for exiting the current state
	CurrentState = NewState;				// Handle setup for entering the new state
	OnEnterState(CurrentState);				// Note: The actual movement logic for each state is handled in the OnEnterState function to ensure clean transitions
}

void AkdMyPlayer::OnEnterState(EPlayerMovementState State)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	switch (State)
	{
		case EPlayerMovementState::Normal3DState:
			Move->SetPlaneConstraintEnabled(false);
			Move->SetMovementMode(MOVE_Walking);
			Move->GravityScale = 1.0f;
			Move->Velocity = FVector::ZeroVector;
			// Ensure capsule snaps back to floor
			Move->bForceNextFloorCheck = true;
			break;

		case EPlayerMovementState::CrushTransitionIn:
			CrushTransition();
			break;

		case EPlayerMovementState::Crush2D_OnGround:
			Move->SetPlaneConstraintEnabled(true);
			Move->SetPlaneConstraintNormal(FVector(1, 0, 0));	// Lock X Movement
			Move->SetMovementMode(MOVE_Walking);
			Move->GravityScale = 1.0f;
			break;

		case EPlayerMovementState::Crush2D_InShadow:
			Move->SetPlaneConstraintEnabled(true);
			Move->SetPlaneConstraintNormal(FVector(1.0f, 0.0f, 0.0f));	// Lock X Movement
			Move->SetMovementMode(MOVE_Flying);	// Disable gravity so player doesn't fall through shadow
			Move->GravityScale = 0.0f;			
			Move->Velocity = FVector::ZeroVector;	// Stop any falling velocity immediately when entering shadow
			break;

		case EPlayerMovementState::CrushTransitionOut:
			// handle in CrushTransition function for smooth interpolation back to 3D
			break;
	}
}

void AkdMyPlayer::OnExitState(EPlayerMovementState State) {	/* Usually empty — FSM keeps things clean */ }

void AkdMyPlayer::UpdateFSM(float DeltaTime)
{
	switch (CurrentState)
	{
	case EPlayerMovementState::Normal3DState:
		// idle
		break;

	case EPlayerMovementState::CrushTransitionIn:
		CrushInterpolation(DeltaTime);
		if (!bIsTransitioning)
		{
			SetMovementState(EPlayerMovementState::Crush2D_OnGround);
		}
		break;

	case EPlayerMovementState::Crush2D_OnGround:
		if (IsStandingInShadow())
		{
			SetMovementState(EPlayerMovementState::Crush2D_InShadow);
		}
		break;

	case EPlayerMovementState::Crush2D_InShadow:
		if (!IsStandingInShadow())
		{
			SetMovementState(EPlayerMovementState::Crush2D_OnGround);
		}
		break;

	case EPlayerMovementState::CrushTransitionOut:
		CrushInterpolation(DeltaTime);
		if (!bIsTransitioning)
		{
			SetMovementState(EPlayerMovementState::Normal3DState);
		}
		break;
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

void AkdMyPlayer::CrushMode()
{
	// MOVEMENT LOGIC
	// Only override physics if we are fully in Crush Mode (2D)
	if (bIsCrushMode && !bIsTransitioning)
	{
		bool bIsOnRealGround = GetCharacterMovement()->IsWalking();
		bool bIsOnShadow = IsStandingInShadow();

		if (bIsOnShadow)
		{
			// CASE: Standing on Shadow
			GetCharacterMovement()->SetMovementMode(MOVE_Flying);			// We disable gravity so the player doesn't fall through the shadow
			GetCharacterMovement()->BrakingDecelerationFlying = 2000.0f;
			GetCharacterMovement()->GravityScale = 0.0f;
			
		}
		else
		{
			// CASE: In Empty Light (Gap)
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);		// If we aren't on ground AND aren't on a shadow, gravity takes over
			GetCharacterMovement()->GravityScale = 1.0f;
		}
	}
	else if (!bIsCrushMode && !bIsTransitioning)
	{
		// Standard 3D behavior
		if (GetCharacterMovement()->MovementMode == MOVE_Flying)
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
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
	// Start the transition
	CrushTransition();		// <-- Use this transition function instead of instant toggle

	//CrushMode();
}
