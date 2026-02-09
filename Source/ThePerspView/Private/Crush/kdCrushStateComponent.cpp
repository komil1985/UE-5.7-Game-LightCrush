// Copyright ASKD Games


#include "Crush/kdCrushStateComponent.h"
#include "Player/kdMyPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"


UkdCrushStateComponent::UkdCrushStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bShowDebugLines = true;
	ShadowCheckFrequency = 0.5f;	
}

void UkdCrushStateComponent::BeginPlay()
{
	Super::BeginPlay();
	FindDirectionalLight();
}

void UkdCrushStateComponent::ToggleShadowTracking(bool bEnable)
{
	AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
	if (!Player) return;

	if (bEnable)
	{
		GetWorld()->GetTimerManager().SetTimer(ShadowTimerHandle, this, &UkdCrushStateComponent::UpdateShadowPhysics, ShadowCheckFrequency, true);
		Player->GetCharacterMovement()->GravityScale = 0.0f;
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(ShadowTimerHandle);
		ResetPhysicsTo3D();
	}
}

void UkdCrushStateComponent::HandleVerticalInput(float Value)
{
	// Allow movement only if in a shadow
	if (IsStandingInShadow())
	{
		AkdMyPlayer* Owner = Cast<AkdMyPlayer>(GetOwner());
		if (Owner)
		{
			UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement();

			// Direct Velocity Control for snappy 2D feel
			FVector CurrentVel = MoveComp->Velocity;
			CurrentVel.Z = Value * ShadowSwimSpeed;
			MoveComp->Velocity = CurrentVel;
		}
	}
}

bool UkdCrushStateComponent::IsStandingInShadow() const
{
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld()) return false;
	
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	FVector Start = Owner->GetActorLocation();
	FVector End = Start + (CachedLightDirection * 50000.0f);

	bool bIsHit = GetWorld()->LineTraceSingleByChannel
	(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	if (bShowDebugLines)
	{
		DrawDebugLine(GetWorld(), Start, bIsHit ? HitResult.ImpactPoint : Start + CachedLightDirection * 1000.0f,
			bIsHit ? FColor::Green : FColor::Red, false, ShadowCheckFrequency * 1.1f, 0, 2.0f);
	}

	return bIsHit;

}

void UkdCrushStateComponent::UpdateShadowPhysics()
{
	AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
	if (!Player) return;

	UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement();

	if (IsStandingInShadow())
	{
		if (MoveComp->MovementMode != MOVE_Flying)
		{
			MoveComp->SetMovementMode(MOVE_Flying);
			MoveComp->BrakingDecelerationFlying = ShadowBrakingDeceleration;
		}
	}
	else
	{
		if (MoveComp->MovementMode != MOVE_Falling)
		{
			MoveComp->SetMovementMode(MOVE_Falling);
		}
	}
}

void UkdCrushStateComponent::FindDirectionalLight()
{
	UWorld* World = GetWorld();
	if (!World)	return;

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
			UE_LOG(LogTemp, Warning, TEXT("CrushStateComponent: No DirectionalLight found!"));
		}
	}
}

void UkdCrushStateComponent::ResetPhysicsTo3D()
{
	if (AkdMyPlayer* Owner = Cast<AkdMyPlayer>(GetOwner()))
	{
		UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement();

		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->GravityScale = 1.0f;
		MoveComp->SetPlaneConstraintEnabled(false);
	}
}

