// Copyright ASKD Games


#include "World/kdShadowPortal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

AkdShadowPortal::AkdShadowPortal()
{
	PrimaryActorTick.bCanEverTick = false;

	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	SetRootComponent(PortalMesh);
	PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(RootComponent);
	TriggerSphere->SetSphereRadius(80.f);
	TriggerSphere->SetCollisionProfileName(TEXT("OverlapAll"));
	TriggerSphere->SetGenerateOverlapEvents(true);
}

void AkdShadowPortal::BeginPlay()
{
	Super::BeginPlay();
	
	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AkdShadowPortal::OnTriggerBeginOverlap);
}

void AkdShadowPortal::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bCanTeleport || !LinkedPortal) return;

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
	if (!Player) return;

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (!ASC) return;

	// Portals are shadow-plane only
	const FkdGameplayTags& StateTags = FkdGameplayTags::Get();
	if (!ASC->HasMatchingGameplayTag(StateTags.State_CrushMode) ||
		!ASC->HasMatchingGameplayTag(StateTags.State_InShadow))
	{
		return;
	}

	// ── Teleport ──────────────────────────────────────────────────────────────
	// Place the player at the linked portal's location offset by ExitOffset
	// (in the linked portal's local space, so the exit direction is always correct).
	const FVector ExitWorldLocation =
		LinkedPortal->GetActorLocation() +
		LinkedPortal->GetActorRotation().RotateVector(LinkedPortal->ExitOffset);

	// ETeleportType::TeleportPhysics: moves actor without triggering sweep collisions,
	// which prevents the player from getting stuck in a wall mid-teleport.
	Player->SetActorLocation(ExitWorldLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Preserve momentum through the portal — keep Y/Z velocity, hard-zero X
	UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement();
	FVector CarriedVelocity = MoveComp->Velocity;
	CarriedVelocity.X = 0.f;
	MoveComp->Velocity = CarriedVelocity;

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("ShadowPortal: Teleported player to %s"), *ExitWorldLocation.ToString());
#endif

	// ── Cooldown ──────────────────────────────────────────────────────────────
	// Disable BOTH portals so the player can't immediately re-enter at the destination.
	bCanTeleport = false;
	LinkedPortal->bCanTeleport = false;

	// Re-enable this portal after cooldown
	FTimerDelegate SelfDelegate;
	SelfDelegate.BindUObject(this, &AkdShadowPortal::EnableTeleport);
	GetWorldTimerManager().SetTimer(
		CooldownTimerHandle,
		SelfDelegate,
		CooldownDuration,
		false);

	// Re-enable the linked portal separately (it has its own timer handle)
	FTimerDelegate LinkedDelegate;
	LinkedDelegate.BindUObject(LinkedPortal.Get(), &AkdShadowPortal::EnableTeleport);
	LinkedPortal->GetWorldTimerManager().SetTimer(
		LinkedPortal->CooldownTimerHandle,
		LinkedDelegate,
		CooldownDuration,
		false);
}

void AkdShadowPortal::EnableTeleport()
{
	bCanTeleport = true;
}

