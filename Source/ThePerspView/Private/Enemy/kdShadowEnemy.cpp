// Copyright ASKD Games


#include "Enemy/kdShadowEnemy.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Components/SphereComponent.h"

// Sets default values
AkdShadowEnemy::AkdShadowEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// No AI controller — movement is driven manually in Tick
	AutoPossessAI = EAutoPossessAI::Disabled;

	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->MaxFlySpeed = PatrolSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->GravityScale = 0.0f;

	DamageSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Damage Sphere"));
	DamageSphere->SetupAttachment(GetRootComponent());
	DamageSphere->SetSphereRadius(GetCapsuleComponent()->GetScaledCapsuleRadius() + 5.0f);
	DamageSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DamageSphere->SetGenerateOverlapEvents(true);
}

void AkdShadowEnemy::BeginPlay()
{
	Super::BeginPlay();
	
	// Derive patrol bounds from spawn location — designer only sets PatrolHalfDistance
	const FVector Origin = GetActorLocation();
	PatrolPointA = FVector(Origin.X, Origin.Y - PatrolHalfDistance, Origin.Z);
	PatrolPointB = FVector(Origin.X, Origin.Y + PatrolHalfDistance, Origin.Z);

	DamageSphere->OnComponentBeginOverlap.AddDynamic(this, &AkdShadowEnemy::OnDamageSphereBeginOverlap);
	DamageSphere->OnComponentEndOverlap.AddDynamic(this, &AkdShadowEnemy::OnDamageSphereEndOverlap);
}

void AkdShadowEnemy::TryApplyContactDamage()
{
	// OverlappingPlayer is a weak pointer — check it hasn't been destroyed
	AkdMyPlayer* Player = OverlappingPlayer.Get();
	if (!Player)
	{
		GetWorldTimerManager().ClearTimer(DamageTickHandle);
		return;
	}

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (!ASC)
	{
		GetWorldTimerManager().ClearTimer(DamageTickHandle);
		return;
	}

	// Tag gate — damage only applies while the player is on the shadow plane.
	// This check runs every tick so if the player exits CrushMode mid-overlap
	// (e.g. stamina exhausted), damage stops immediately on the next tick.
	const FkdGameplayTags& StateTags = FkdGameplayTags::Get();
	if (!ASC->HasMatchingGameplayTag(StateTags.State_CrushMode) ||
		!ASC->HasMatchingGameplayTag(StateTags.State_InShadow))
	{
		// Player is no longer on the shadow plane — skip this tick but keep
		// the timer running so damage resumes if they re-enter the shadow.
		return;
	}

	// Apply the contact effect
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ContactEffectClass, 1, Context);
	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("ShadowEnemy [%s]: Damage tick applied to player"), *GetName());
#endif
	}
}

void AkdShadowEnemy::OnDamageSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only track one player at a time
	if (OverlappingPlayer.IsValid()) return;

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
	if (!Player || !ContactEffectClass) return;

	// Store the overlapping player
	OverlappingPlayer = Player;

	// Apply damage immediately on first contact, then start the repeating tick.
	// This means the player feels the hit right away rather than waiting one interval.
	TryApplyContactDamage();

	// Start a repeating timer — fires TryApplyContactDamage every DamageTickRate seconds.
	// The timer keeps running until EndOverlap clears it, so damage applies
	// continuously while the player stays inside the sphere.
	GetWorldTimerManager().SetTimer(
		DamageTickHandle,
		this, &AkdShadowEnemy::TryApplyContactDamage,
		DamageTickRate,
		true);  // true = looping
}

void AkdShadowEnemy::OnDamageSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// Only react to the tracked player leaving — ignore other actors
	if (OtherActor != OverlappingPlayer.Get()) return;

	// Player has left the sphere — stop the damage timer and clear the reference.
	// No more damage until they re-enter and trigger BeginOverlap again.
	GetWorldTimerManager().ClearTimer(DamageTickHandle);
	OverlappingPlayer = nullptr;

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("ShadowEnemy [%s]: Player left sphere, damage stopped"), *GetName());
#endif
}

void AkdShadowEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const FVector& Target = bMovingToB ? PatrolPointB : PatrolPointA;
	const FVector  CurrentLoc = GetActorLocation();
	const float    DistY = FMath::Abs(Target.Y - CurrentLoc.Y);

	if (DistY < 5.f)
	{
		bMovingToB = !bMovingToB;
	}
	else
	{
		const float   StepY = FMath::Sign(Target.Y - CurrentLoc.Y) * PatrolSpeed * DeltaTime;
		const FVector NewLoc = FVector(CurrentLoc.X, CurrentLoc.Y + StepY, CurrentLoc.Z);

		// ── WHY bSweep=false ─────────────────────────────────────────────────
		// bSweep=true causes UE to move the actor via a physics sweep each tick.
		// A sweep re-evaluates all overlaps along the path — so if the player is
		// already inside DamageSphere, every single movement tick generates a NEW
		// OnComponentBeginOverlap event, firing TryApplyContactDamage every frame.
		// bSweep=false teleports the actor to the new position without a sweep,
		// so BeginOverlap only fires when the player ENTERS the sphere fresh.
		// Blocking geometry is handled separately by the capsule's blocking profile.
		SetActorLocation(NewLoc, false, nullptr, ETeleportType::None);
	}
}

