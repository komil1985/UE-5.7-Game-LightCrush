// Copyright ASKD Games


#include "Crush/kdCrushStateComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"


UkdCrushStateComponent::UkdCrushStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UkdCrushStateComponent::BeginPlay()
{
	Super::BeginPlay();
	PlayerCharacter = Cast<ACharacter>(GetOwner());
}

void UkdCrushStateComponent::EnterCrushState()
{
	if (!PlayerCharacter || bIsCrushed) return;

	bIsCrushed = true;
	ConfigureMovementForCrush(true);
	ApplyCrushEffect();
}

void UkdCrushStateComponent::ExitCrushState()
{
	if (!PlayerCharacter || !bIsCrushed) return;

	bIsCrushed = false;
	ConfigureMovementForCrush(false);
	RemoveCrushEffect();

}

void UkdCrushStateComponent::ConfigureMovementForCrush(bool bEnable)
{
	UCharacterMovementComponent* MoveComp = PlayerCharacter->GetCharacterMovement();
	if (!MoveComp) return;

	if (bEnable)
	{
		MoveComp->SetPlaneConstraintEnabled(true);
		MoveComp->SetPlaneConstraintNormal(FVector(1, 0, 0));	// Lock X axis
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->GravityScale = 1.0f;
	}
	else
	{
		MoveComp->SetPlaneConstraintEnabled(false);
		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->GravityScale = 1.0f;
	}

}

void UkdCrushStateComponent::ApplyCrushEffect()
{
	if (!PlayerCharacter) return;

	UCameraComponent* Camera = PlayerCharacter->FindComponentByClass<UCameraComponent>();
	if (!Camera) return;
}

void UkdCrushStateComponent::RemoveCrushEffect()
{
	if (!PlayerCharacter || !CrushPPInstance) return;

	UCameraComponent* Camera = PlayerCharacter->FindComponentByClass<UCameraComponent>();
	if (!Camera) return;

	CrushPPInstance = nullptr;
}
