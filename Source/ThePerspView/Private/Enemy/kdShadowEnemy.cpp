// Copyright ASKD Games


#include "Enemy/kdShadowEnemy.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"

// Sets default values
AkdShadowEnemy::AkdShadowEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// No AI controller — movement is driven manually in Tick
	AutoPossessAI = EAutoPossessAI::Disabled;

	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AkdShadowEnemy::BeginPlay()
{
	Super::BeginPlay();
	
	// Derive patrol bounds from spawn location — designer only sets PatrolHalfDistance
	const FVector Origin = GetActorLocation();
	PatrolPointA = FVector(Origin.X, Origin.Y - PatrolHalfDistance, Origin.Z);
	PatrolPointB = FVector(Origin.X, Origin.Y + PatrolHalfDistance, Origin.Z);

	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &AkdShadowEnemy::OnCapsuleBeginOverlap);
}

void AkdShadowEnemy::OnCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bCanDamage || !ContactEffectClass) return;

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
	if (!Player) return;

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (!ASC) return;

	// Only dangerous while the player is in shadow mode
	const FkdGameplayTags& StateTags = FkdGameplayTags::Get();
	if (!ASC->HasMatchingGameplayTag(StateTags.State_CrushMode) ||
		!ASC->HasMatchingGameplayTag(StateTags.State_InShadow))
	{
		return;
	}

	// Apply the contact damage effect to the player
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ContactEffectClass, 1, Context);
	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("ShadowEnemy: Hit player, applied contact effect"));
#endif
	}

	// Start per-hit cooldown so the player has time to react
	bCanDamage = false;
	GetWorldTimerManager().SetTimer(
		DamageCooldownHandle,
		this, &AkdShadowEnemy::ResetDamageCooldown,
		ContactCooldown,
		false);
}

void AkdShadowEnemy::ResetDamageCooldown()
{
	bCanDamage = true;
}

void AkdShadowEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── Patrol ────────────────────────────────────────────────────────────────
	const FVector& Target = bMovingToB ? PatrolPointB : PatrolPointA;
	const FVector  CurrentLoc = GetActorLocation();
	const float    DistToTarget = FVector::Dist2D(CurrentLoc, Target);

	if (DistToTarget < 30.f)
	{
		// Close enough — flip direction
		bMovingToB = !bMovingToB;
	}
	else
	{
		// Move toward current target along Y only — X/Z are never changed by this
		const FVector Direction = (Target - CurrentLoc).GetSafeNormal2D();
		AddMovementInput(FVector(0.f, Direction.Y > 0.f ? 1.f : -1.f, 0.f));
	}
}

