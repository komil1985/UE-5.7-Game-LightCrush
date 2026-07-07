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
#include "Components/kdLightHealthComponent.h"
#include "UI/Widget/kdLightHealthWidget.h"
#include "Crush/kdCrushDirectionLibrary.h"
#include "AbilitySystem/Abilities/kdStrategicView.h"
#include "Components/kdStrategicCameraComponent.h"
#include "Components/kdJumpSquashComponent.h"
#include "NiagaraComponent.h"



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

	Tentacle_1 = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Tentacle 1"));
	Tentacle_1->SetupAttachment(GetMesh());

	Tentacle_2 = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Tentacle 2"));
	Tentacle_2->SetupAttachment(GetMesh());

	Tentacle_3 = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Tentacle 3"));
	Tentacle_3->SetupAttachment(GetMesh());

	Tentacle_4 = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Tentacle 4"));
	Tentacle_4->SetupAttachment(GetMesh());

	CrushStateComponent = CreateDefaultSubobject<UkdCrushStateComponent>(TEXT("CrushState"));
	CrushTransitionComponent = CreateDefaultSubobject<UkdCrushTransitionComponent>(TEXT("CrushTransition"));
	DeathComponent = CreateDefaultSubobject<UkdDeathComponent>(TEXT("DeathComponent"));
	FallDamageComponent = CreateDefaultSubobject<UkdFallDamageComponent>(TEXT("FallDamageComponent"));
	GameFeedbackComponent = CreateDefaultSubobject<UkdGameFeedbackComponent>(TEXT("GameFeedbackComponent"));
	HoverComponent = CreateDefaultSubobject<UkdPlayerHoverComponent>(TEXT("HoverComponent"));
	//LightHealthComponent = CreateDefaultSubobject<UkdLightHealthComponent>(TEXT("LightHealthComponent"));
	JumpSquashComponent = CreateDefaultSubobject<UkdJumpSquashComponent>(TEXT("JumpSquashComponent"));
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

	// Low Stamina Warning Widget Component — world-space, floats above the player's head.
	// NOTE: this is the legacy world-space UI pattern; candidate for migration into the
	// screen-space HUD (see chat notes). Left active so the flash warning keeps working.
	LowStaminaWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("LowStaminaWidgetComponent"));
	LowStaminaWidgetComponent->SetupAttachment(GetMesh());
	LowStaminaWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	LowStaminaWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);

	/* -- Strategic Camera Component -- */
	StrategicCameraComponent = CreateDefaultSubobject<UkdStrategicCameraComponent>(TEXT("StrategicCameraComponent"));
	/*-----------------------------------------------------------------------------------------------------------*/

}

UAbilitySystemComponent* AkdMyPlayer::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AkdMyPlayer::HandleLevelComplete()
{
	// Light health: stop the drain/heal tick + danger flash, then hide the widget.
	if (LightHealthComponent) LightHealthComponent->Freeze();
	if (LightHealthWidget)    LightHealthWidget->HideWidget();

	// Stamina: stop the Crush-Mode drain tick and any pending/active regen.
	if (UkdCrushStateComponent* Crush = FindComponentByClass<UkdCrushStateComponent>())
	{
		Crush->Freeze();
	}
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
		AbilitySystemComponent->RegisterGameplayTagEvent(FkdGameplayTags::Get().State_CrushMode, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AkdMyPlayer::OnCrushModeTagChanged);
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

	// Tentacles react to launch/fall/land off the same curves driving the body squash.
	if (JumpSquashComponent)
	{
		JumpSquashComponent->SetTentacleComponents({ Tentacle_1, Tentacle_2, Tentacle_3, Tentacle_4 });
	}

	// ── Light Health Widget — created as a screen-space viewport widget ───────
	//if (LightHealthWidgetClass)
	//{
	//	APlayerController* PC = Cast<APlayerController>(GetController());
	//	if (!PC) PC = GetWorld()->GetFirstPlayerController();

	//	LightHealthWidget = CreateWidget<UkdLightHealthWidget>(PC, LightHealthWidgetClass);
	//	if (LightHealthWidget)
	//	{
	//		LightHealthWidget->AddToViewport(5);		// ZOrder 5: above gameplay, below menus
	//		LightHealthWidget->SetVisibility(ESlateVisibility::Hidden);		// hidden until CrushMode
	//		LightHealthWidget->InitializeWithASC(AbilitySystemComponent, LightHealthComponent);

	//		UE_LOG(LogTemp, Log, TEXT("LightHealthWidget: Created and added to viewport."));
	//	}
	//	else
	//	{
	//		UE_LOG(LogTemp, Error,
	//			TEXT("LightHealthWidget: CreateWidget FAILED. Ensure WBP_LightHealth parent class is UkdLightHealthWidget."));
	//	}
	//}
	//else
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("LightHealthWidgetClass not assigned in BP_Player Details panel!"));
	//}

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
				if (!bActivated && AbilitySystemComponent->HasMatchingGameplayTag(FkdGameplayTags::Get().State_Exhausted))
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

void AkdMyPlayer::RequestStrategicViewStart()
{
	if (!AbilitySystemComponent) return;

	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(UkdStrategicView::StaticClass()))
		{
			if (FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
			{
				AbilitySystemComponent->TryActivateAbility(Spec->Handle);
			}
			return;
		}
	}
}

void AkdMyPlayer::RequestStrategicViewStop()
{
	if (!AbilitySystemComponent) return;

	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(UkdStrategicView::StaticClass()))
		{
			if (FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
			{
				AbilitySystemComponent->CancelAbilityHandle(Spec->Handle);
			}
			return;
		}
	}
}

void AkdMyPlayer::OnTransitionFinished(bool bNewCrushState)
{
	if (!AbilitySystemComponent) return;

	if (bNewCrushState)
	{
		// ── Entering 2D ───────────────────────────────────────────────────────
		// Lock the COLLAPSE axis for the active direction (X for N/S, Y for E/W),
		// through the player's current position. The other two axes stay free.
		UCharacterMovementComponent* MoveComp = GetCharacterMovement();

		const FVector CollapseN = UkdCrushDirectionLibrary::MakeCrushBasis(GetActiveCrushDirection()).CollapseNormal;
		MoveComp->SetPlaneConstraintNormal(CollapseN);
		MoveComp->SetPlaneConstraintOrigin(GetActorLocation());
		MoveComp->SetPlaneConstraintEnabled(true);
	}
	else
	{
		// ── Exiting 2D ────────────────────────────────────────────────────────
		// Player's X never changed, Y/Z are wherever they moved in 2D.
		// Just release the constraint — position is already correct.
		GetCharacterMovement()->SetPlaneConstraintEnabled(false);
	}
}

void AkdMyPlayer::OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// ── Widget visibility ─────────────────────────────────────────────────────
	// MUST be first — before the State_Transitioning guard below.
	// When entering Crush Mode, State_CrushMode is added while State_Transitioning
	// is still active (EndAbility hasn't fired yet), so anything after the guard
	// is never reached on enter. Widget visibility must never depend on that guard.
	if (LightHealthWidget)
	{
		if (NewCount > 0)
			LightHealthWidget->ShowWidget();
		else
			LightHealthWidget->HideWidget();
	}

	// ── Transition guard ──────────────────────────────────────────────────────
	// Only protects StartTransition(false) from double-firing during a live
	// transition. Everything above this line runs unconditionally.
	if (AbilitySystemComponent->HasMatchingGameplayTag(FkdGameplayTags::Get().State_Transitioning))
	{
		return;
	}

	if (NewCount == 0)
	{
		// Exiting Crush Mode — start the 3D camera/mesh transition
		if (CrushTransitionComponent)
		{
			CrushTransitionComponent->StartTransition(false);
		}
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

void AkdMyPlayer::NotifyJumpApex()
{
	Super::NotifyJumpApex();

	if (JumpSquashComponent)
	{
		JumpSquashComponent->NotifyApex();
	}
}