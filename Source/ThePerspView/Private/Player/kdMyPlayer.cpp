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
#include "UI/Widget/kdStaminaWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/kdCharacterMovementComponent.h"
#include "AbilitySystem/Abilities/kdShadowDash.h"
#include "Interfaces/kdInteractable.h"
#include "Engine/OverlapResult.h"
#include "Components/kdDeathComponent.h"
#include "Components/kdFallDamageComponent.h"
#include "Components/kdGameFeedbackComponent.h"
#include "UI/Widget/kdLowStaminaWidget.h"
#include "Components/kdPlayerHoverComponent.h"




AkdMyPlayer::AkdMyPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UkdCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;

	/*	--	Components	--	*/
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 1000.0f;
	SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = true;
	SpringArm->ProbeSize = 12.0f;
	SpringArm->ProbeChannel = ECC_Camera;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.0f;
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	EyeLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftEye"));
	EyeLeft->SetupAttachment(GetMesh());
	EyeLeft->SetRelativeLocation(FVector(150.0f, 50.0f, 50.0f));

	EyeRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightEye"));
	EyeRight->SetupAttachment(GetMesh());
	EyeRight->SetRelativeLocation(FVector(150.0f, -50.0f, 50.0f));	

	CrushStateComponent = CreateDefaultSubobject<UkdCrushStateComponent>(TEXT("CrushState"));
	CrushTransitionComponent = CreateDefaultSubobject<UkdCrushTransitionComponent>(TEXT("CrushTransition"));
	DeathComponent = CreateDefaultSubobject<UkdDeathComponent>(TEXT("DeathComponent"));
	FallDamageComponent = CreateDefaultSubobject<UkdFallDamageComponent>(TEXT("FallDamageComponent"));
	GameFeedbackComponent = CreateDefaultSubobject<UkdGameFeedbackComponent>(TEXT("GameFeedbackComponent"));
	HoverComponent = CreateDefaultSubobject<UkdPlayerHoverComponent>(TEXT("HoverComponent"));
	/*-----------------------------------------------------------------------------------------------------------*/

	/*	--	Default Values	--	*/
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	/*-----------------------------------------------------------------------------------------------------------*/

	/* -- GAS Setup -- */	
	AbilitySystemComponent = CreateDefaultSubobject<UkdAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false); // Essential for multiplayer, safe for single
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UkdAttributeSet>(TEXT("AttributeSet"));
	/*-----------------------------------------------------------------------------------------------------------*/

	/* -- Stamina Widget Component -- */
	StaminaWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("StaminaWidgetComponent"));
	StaminaWidgetComponent->SetupAttachment(GetMesh());
	StaminaWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	StaminaWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);

	// Low Stamina Warning Widget — floats above the stamina bar
	LowStaminaWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("LowStaminaWidgetComponent"));
	LowStaminaWidgetComponent->SetupAttachment(GetMesh());
	LowStaminaWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));  // above StaminaWidgetComponent
	LowStaminaWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
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
	if (CrushTransitionComponent) CrushTransitionComponent->OnTransitionComplete.AddDynamic(this, &AkdMyPlayer::OnTransitionFinished);
	
	// Initialize AbilitySystem
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		InitializeAbilitySystem();

		// Bind to tag changes for CrushMode
		AbilitySystemComponent->RegisterGameplayTagEvent(FkdGameplayTags::Get().State_CrushMode,EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AkdMyPlayer::OnCrushModeTagChanged);
	}

	//--- Stamina Widget ---------------------------------------------------
	if (StaminaWidgetComponent && StaminaWidgetClass)
	{
		StaminaWidgetComponent->SetWidgetClass(StaminaWidgetClass);
		// Optional: force widget creation if not already created
		StaminaWidgetComponent->InitWidget();

		UUserWidget* Widget = StaminaWidgetComponent->GetUserWidgetObject();
		if (StaminaWidget = Cast<UkdStaminaWidget>(Widget))
		{
			StaminaWidget->InitializeWithAbilitySystemComponent(AbilitySystemComponent);
		}
	}
	
	// ── Low Stamina Warning Widget ──────────────────────────────────────────────
	if (LowStaminaWidgetComponent && LowStaminaWidgetClass)
	{
		LowStaminaWidgetComponent->SetWidgetClass(LowStaminaWidgetClass);
		LowStaminaWidgetComponent->InitWidget();

		if (LowStaminaWidget = Cast<UkdLowStaminaWidget>(LowStaminaWidgetComponent->GetUserWidgetObject()))
		{
			// Hidden by default — only revealed when FlashWarning() is called
			LowStaminaWidget->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	// Both eyes blink in sync; add or remove entries freely.
	if (HoverComponent)
	{
		HoverComponent->SetMeshComponents(GetMesh(), { EyeLeft, EyeRight });  // Pass both eye meshes
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
				const bool bActivated = AbilitySystemComponent->TryActivateAbility(Spec->Handle);
#if !UE_BUILD_SHIPPING
				UE_LOG(LogTemp, Log, TEXT("RequestCrushToggle: TryActivateAbility returned %d"), bActivated);
#endif
				// TryActivateAbility returns false for ANY block reason (transitioning,
				// exhausted, etc.).  We only want the warning when the specific cause
				// is stamina exhaustion — check the tag directly.
				if (!bActivated &&	AbilitySystemComponent->HasMatchingGameplayTag(FkdGameplayTags::Get().State_Exhausted))
				{
					NotifyLowStaminaWarning();
				}
			}
			else
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(LogTemp, Warning, TEXT("RequestCrushToggle: Could not find ability spec for class %s"), *AbilityClass->GetName());
#endif
			}
			return;
		}
	}
}

void AkdMyPlayer::RequestInteract()
{
	if (!AbilitySystemComponent) return;

	// Only allow interaction in Crush Mode (matches Interact_CrushOnly intent)
	if (!AbilitySystemComponent->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
		return;

	// Sphere overlap from player location — finds all IkdInteractable actors within reach
	const float InteractRadius = 200.f;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	GetWorld()->OverlapMultiByChannel(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(InteractRadius),
		Params);

	// Find the nearest interactable
	float    BestDist = MAX_FLT;
	AActor* BestActor = nullptr;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && Actor->Implements<UkdInteractable>())
		{
			const float Dist = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
			if (Dist < BestDist)
			{
				BestDist = Dist;
				BestActor = Actor;
			}
		}
	}

	if (BestActor)
	{
		// Pure C++ virtual interface — cast directly, no Execute_ wrapper needed.
		// Execute_ is only generated by UE for BlueprintNativeEvent functions.
		if (IkdInteractable* Interactable = Cast<IkdInteractable>(BestActor))
		{
			Interactable->Interact(this);
		}
	}
}

void AkdMyPlayer::RequestShadowDash()
{
	if (!AbilitySystemComponent) return;

	// Mirror of RequestCrushToggle — find the dash ability spec and activate it
	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(UkdShadowDash::StaticClass()))
		{
			FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass);
			if (Spec)
			{
				AbilitySystemComponent->TryActivateAbility(Spec->Handle);
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
	if (AbilitySystemComponent->HasMatchingGameplayTag(FkdGameplayTags::Get().State_Transitioning))
	{
		return;
	}

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
}

void AkdMyPlayer::NotifyLowStaminaWarning()
{
	if (LowStaminaWidget)
	{
		LowStaminaWidget->FlashWarning();
	}
}
