// Copyright ASKD Games


#include "Crush/kdCrushStateComponent.h"
#include "Player/kdMyPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"

UkdCrushStateComponent::UkdCrushStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bShowDebugLines = true;	
}

void UkdCrushStateComponent::BeginPlay()
{
	Super::BeginPlay();
	CachedOwner = Cast<AkdMyPlayer>(GetOwner());
	FindDirectionalLight();

	LastShadowCheckTime = 0.0f;
}

void UkdCrushStateComponent::ToggleShadowTracking(bool bEnable)
{
	if (!CachedOwner) return;

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();

	if (bEnable)
	{
		if (TimerManager.TimerExists(ShadowTimerHandle))
		{
			TimerManager.ClearTimer(ShadowTimerHandle);
		}
		TimerManager.SetTimer(ShadowTimerHandle, this, &UkdCrushStateComponent::UpdateShadowPhysics, ShadowCheckFrequency, true);
	}
	else
	{
		TimerManager.ClearTimer(ShadowTimerHandle);
		ResetPhysicsTo3D();
	}
}

void UkdCrushStateComponent::HandleVerticalInput(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;

	// Allow upward movement only if in a shadow
	if (bIsInShadow)
	{
		if (CachedOwner)
		{
			bCanMoveInShadow = true;
			CachedOwner->LaunchCharacter(FVector(0, 0, Value * ShadowMoveSpeed), true, true);
		}
	}
	bCanMoveInShadow = false;
}

bool UkdCrushStateComponent::IsStandingInShadow() const
{
	if (!CachedOwner || !GetWorld()) return false;
	
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CachedOwner);

	FVector Start = CachedOwner->GetActorLocation();
	FVector End = Start + (CachedLightDirection * ShadowTraceDistance);

	bool bIsHit = GetWorld()->LineTraceSingleByChannel
	(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		Params
	);

#if !UE_BUILD_SHIPPING
	if (bShowDebugLines)
	{
		DrawDebugLine(GetWorld(), Start, bIsHit ? HitResult.ImpactPoint : Start + CachedLightDirection * 1000.0f,
			bIsHit ? FColor::Green : FColor::Red, false, ShadowCheckFrequency * 1.1f, 0, 2.0f);
	}
#endif

	return bIsHit;

}

void UkdCrushStateComponent::UpdateShadowPhysics()
{
	if (!CachedOwner || !GetWorld()) return;

	// Adaptive throttle: compute desired interval based on movement
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// Determine if player is moving (small threshold)
	const float MovementSpeed = CachedOwner->GetVelocity().Size();
	const bool bIsMoving = MovementSpeed > 1.0f;

	// Choose interval
	const float DesiredInterval = bIsMoving ? ShadowCheckFrequencyMoving : ShadowCheckFrequency;

	// Skip if called too soon
	if (CurrentTime - LastShadowCheckTime < DesiredInterval)
	{
		return;
	}
	LastShadowCheckTime = CurrentTime;

	// Only run shadow logic when in crush mode or when moving and likely to enter shadow
	UAbilitySystemComponent* ASC = CachedOwner->GetAbilitySystemComponent();
	if (!ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode) && !bIsMoving)
	{
		// If not in crush mode and not moving, ensure physics are normal and skip trace
		if (CachedOwner->GetCharacterMovement()->GravityScale != 1.0f)
		{
			CachedOwner->GetCharacterMovement()->GravityScale = 1.0f;
		}
		bIsInShadow = false;
		bCanMoveInShadow = false;
		return;
	}
	const FkdGameplayTags& Tags = FkdGameplayTags::Get();
	bIsInShadow = IsStandingInShadow();

	if (bIsInShadow)
	{
		CachedOwner->GetCharacterMovement()->GravityScale = 0.25f;
		ASC->AddLooseGameplayTag(Tags.State_InShadow);
	}
	else
	{
		CachedOwner->GetCharacterMovement()->GravityScale = 1.0f;
		ASC->RemoveLooseGameplayTag(Tags.State_InShadow);
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
		#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Warning, TEXT("CrushStateComponent: No DirectionalLight found!"));
		#endif
		}
	}
}

void UkdCrushStateComponent::ResetPhysicsTo3D()
{
	if (CachedOwner)
	{
		CachedOwner->GetCharacterMovement()->GravityScale = 1.0f;
		CachedOwner->GetCharacterMovement()->SetPlaneConstraintEnabled(false);
	}
}
