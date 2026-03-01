// Copyright ASKD Games


#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Crush/kdCrushStateComponent.h"
#include "Crush/kdCrushTransitionComponent.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystem/Abilities/kd_CrushToggle.h"

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

	//bIsCrushMode = false;
	//bIsTransitioning = false;
	/*-----------------------------------------------------------------------------------------------------------*/

	/* -- GAS Setup -- */	
	AbilitySystemComponent = CreateDefaultSubobject<UkdAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false); // Essential for multiplayer, safe for single
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UkdAttributeSet>(TEXT("AttributeSet"));
	/*-----------------------------------------------------------------------------------------------------------*/

}

UAbilitySystemComponent* AkdMyPlayer::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AkdMyPlayer::BeginPlay()
{
	Super::BeginPlay();

	// Binding Transition Finished Event
	if (CrushTransitionComponent)	CrushTransitionComponent->OnTransitionComplete.AddDynamic(this, &AkdMyPlayer::OnTransitionFinished);
	
	// Initialize GAS
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		InitializeAbilitySystem();
	}

}

void AkdMyPlayer::RequestCrushToggle()
{
	if (!AbilitySystemComponent) return;

	// Find the CrushToggle ability class in the DefaultAbilities array
	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(Ukd_CrushToggle::StaticClass()))
		{
			AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
			return;
		}
	}

	// Fallback: try by tag (if you still want it)
	FGameplayTagContainer AbilityTag;
	AbilityTag.AddTag(FkdGameplayTags::Get().Ability_LightCrush);
	AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTag);
}

void AkdMyPlayer::OnTransitionFinished(bool bNewCrushState)
{
	if (!AbilitySystemComponent) return;

	const FkdGameplayTags& MyTags = FkdGameplayTags::Get();
	bool bInCrushMode = AbilitySystemComponent->HasMatchingGameplayTag(MyTags.State_CrushMode);

#if UE_BUILD_SHIPPING
	// Log state
	UE_LOG(LogTemp, Log, TEXT("OnTransitionFinished - bNewCrushState=%d, bInCrushMode=%d"), bNewCrushState, bInCrushMode);
#endif

	// Tell the physics engine to start/stop tracking shadows
	if (CrushStateComponent) CrushStateComponent->ToggleShadowTracking(AbilitySystemComponent->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode));
	
	// Set Plane constraints for 2D movement
	if (AbilitySystemComponent->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
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
		GetCharacterMovement()->SetPlaneConstraintNormal(FVector::ZeroVector); // important!
	}

#if UE_BUILD_SHIPPING
	// Log final constraint state
	UE_LOG(LogTemp, Log, TEXT("PlaneConstraintEnabled: %d"), GetCharacterMovement()->GetPlaneConstraintEnabled());
#endif
}

void AkdMyPlayer::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Error, TEXT("InitializeAbilitySystem: No AbilitySystemComponent!"));
#endif
		return;
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("InitializeAbilitySystem: Starting with %d default abilities"), DefaultAbilities.Num());
#endif

	// Grant startup abilities (Crush, Jump, etc.)
	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
			AbilitySystemComponent->GiveAbility(Spec);

#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Log, TEXT("Granted ability: %s"), *AbilityClass->GetName());
#endif
		}
		else
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Warning, TEXT("Found null ability class in DefaultAbilities array"));
#endif
		}
	}

	// List all granted abilities after giving them
	const TArray<FGameplayAbilitySpec>& Specs = AbilitySystemComponent->GetActivatableAbilities();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Total activatable abilities after grant: %d"), Specs.Num());
#endif
	for (const FGameplayAbilitySpec& Spec : Specs)
	{
		if (Spec.Ability)
		{
			UE_LOG(LogTemp, Log, TEXT(" - Active ability: %s"), *Spec.Ability->GetName());

			// Check if this ability has the LightCrush tag
			const FGameplayTagContainer& AbilityTags = Spec.Ability->AbilityTags;
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Log, TEXT("   Tags: %s"), *AbilityTags.ToStringSimple());
#endif
		}
	}

	// Initialize attributes with default values
	if (AttributeSet)
	{
		AbilitySystemComponent->GetSpawnedAttributes_Mutable().Add(AttributeSet);

		// Set initial stamina values
		AttributeSet->SetMaxShadowStamina(100.0f);
		AttributeSet->SetShadowStamina(100.0f);
	}
}
 
void AkdMyPlayer::RequestVerticalMove(const FInputActionValue& Value)
{
/*	if (CrushStateComponent)
	{
		CrushStateComponent->HandleVerticalInput(Value.Get<float>());
	}*/
}
