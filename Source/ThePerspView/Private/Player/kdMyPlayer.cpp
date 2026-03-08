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
#include "AbilitySystem/Abilities/kdShadowMove.h"

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

		// Bind to tag changes for CrushMode
		AbilitySystemComponent->RegisterGameplayTagEvent(
			FkdGameplayTags::Get().State_CrushMode,
			EGameplayTagEventType::NewOrRemoved
		).AddUObject(this, &AkdMyPlayer::OnCrushModeTagChanged);
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
			FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass);
			if (Spec)
			{
				bool bActivated = AbilitySystemComponent->TryActivateAbility(Spec->Handle);
				UE_LOG(LogTemp, Log, TEXT("RequestCrushToggle: TryActivateAbility returned %d"), bActivated);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("RequestCrushToggle: Could not find ability spec for class %s"), *AbilityClass->GetName());
			}
			return;
		}
	}
}

void AkdMyPlayer::OnTransitionFinished(bool bNewCrushState)
{
	if (!AbilitySystemComponent) return;

	// Set Plane constraints for 2D movement
	if (bNewCrushState)
	{
		// Align Player to (X) axis
		FVector NewLocation = GetActorLocation();
		NewLocation.X = PlaneConstraintXValue;
		SetActorLocation(NewLocation);

		GetCharacterMovement()->SetPlaneConstraintNormal(PlaneConstraintNormal);
		GetCharacterMovement()->SetPlaneConstraintEnabled(true);
	}
	else
	{
		GetCharacterMovement()->SetPlaneConstraintEnabled(false);
	}
	// Tell the physics engine to start/stop tracking shadows
	if (CrushStateComponent) CrushStateComponent->ToggleShadowTracking(bNewCrushState);
}

void AkdMyPlayer::OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount == 0)
	{
		// Crush mode was removed – revert to 3D
		if (CrushTransitionComponent)
		{
			CrushTransitionComponent->StartTransition(false);
		}
		// Also ensure physics are reset (ToggleShadowTracking will be called when transition finishes)
		// Optionally, you can force an immediate reset here if needed, but better to let the transition handle it.
	}
}

void AkdMyPlayer::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent) return;

	// Grant startup abilities (Crush, Jump, etc.)
	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
			AbilitySystemComponent->GiveAbility(Spec);
		}
	}

	// List all granted abilities after giving them
	const TArray<FGameplayAbilitySpec>& Specs = AbilitySystemComponent->GetActivatableAbilities();
	for (const FGameplayAbilitySpec& Spec : Specs)
	{
		if (Spec.Ability)
		{
			 //Check if this ability has the LightCrush tag
			const FGameplayTagContainer& AbilityTags = Spec.Ability->AbilityTags;
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

	if (AbilitySystemComponent)
	{
		// load or reference your regen effect
		if (RegenEffectClass)
		{
			FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
			EffectContext.AddSourceObject(this);
			FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(RegenEffectClass, 1, EffectContext);
			if (SpecHandle.IsValid())
			{
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to find GE_ShadowStaminaRegen class!"));
		}
	}
}

void AkdMyPlayer::RequestVerticalMove()
{
	//if (AbilitySystemComponent)
	//{
	//	AbilitySystemComponent->TryActivateAbilityByClass(UkdShadowMove::StaticClass());
	//}

	if (!AbilitySystemComponent) return;

	// Find the CrushToggle ability class in the DefaultAbilities array
	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(UkdShadowMove::StaticClass()))
		{
			AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
			return;
		}
	}
}
