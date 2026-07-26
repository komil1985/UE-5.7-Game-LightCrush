// Copyright ASKD Games


#include "World/kdShadowPortal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/kdPortalVortexComponent.h"
#include "Components/kdPortalTeleportComponent.h"


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

	Vortex = CreateDefaultSubobject<UkdPortalVortexComponent>(TEXT("Vortex"));

	// Portal starts completely hidden and inactive.
	// SetPortalActive(true) is called when the player enters CrushMode.
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AkdShadowPortal::BeginPlay()
{
	Super::BeginPlay();
	
	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AkdShadowPortal::OnTriggerBeginOverlap);

	// Find the local player and register a tag-change callback so the portal
	// appears and disappears in sync with CrushMode — no polling needed.
	AkdMyPlayer* Player = Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

	if (Player)
	{
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		if (ASC)
		{
			ASC->RegisterGameplayTagEvent(FkdGameplayTags::Get().State_CrushMode,EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AkdShadowPortal::OnCrushModeTagChanged);

			// Sync immediately in case the player is already in CrushMode
			// (e.g. portal placed in a level that starts in 2D).
			Vortex->InitializeForMesh(PortalMesh, ASC);

			const bool bAlreadyCrushing = ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode);
			SetPortalActive(bAlreadyCrushing);
			if (bAlreadyCrushing) { Vortex->OnPortalShown(); }
		}
	}
}

void AkdShadowPortal::OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// ── Crush Mode Visibility ─────────────────────────────────────────────────────
	// NewCount > 0  → CrushMode just became active   → show portal
	// NewCount == 0 → CrushMode was just removed      → hide portal
	SetPortalActive(NewCount > 0);
}

void AkdShadowPortal::SetPortalActive(bool bActive)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);

	if (Vortex) { bActive ? Vortex->OnPortalShown() : Vortex->OnPortalHidden(); }

	// Also reset teleport state when hiding so stale cooldowns don't persist
	if (!bActive)
	{
		bCanTeleport = true;
		GetWorldTimerManager().ClearTimer(CooldownTimerHandle);
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("ShadowPortal [%s]: SetPortalActive=%d"),*GetName(), bActive);
#endif
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
	if (!ASC->HasMatchingGameplayTag(StateTags.State_CrushMode) || !ASC->HasMatchingGameplayTag(StateTags.State_InShadow))
	{
		return;
	}

	// ── Teleport ──────────────────────────────────────────────────────────────
	// Place the player at the linked portal's location offset by ExitOffset
	// (in the linked portal's local space, so the exit direction is always correct).
	const FVector ExitWorldLocation = LinkedPortal->GetActorLocation() + LinkedPortal->GetActorRotation().RotateVector(LinkedPortal->ExitOffset);

	// ETeleportType::TeleportPhysics: moves actor without triggering sweep collisions,
	// which prevents the player from getting stuck in a wall mid-teleport.
	//Player->SetActorLocation(ExitWorldLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Hand the smooth move to the player. It fades, moves under cover, and calls
	// LinkedPortal->OnPlayerArrived() at the covered moment for the exhale.
	if (UkdPortalTeleportComponent* TC = Player->FindComponentByClass<UkdPortalTeleportComponent>())
	{
		if (TC->IsTeleporting()) { return; }        // guard double-triggers
		TC->BeginTeleport(ExitWorldLocation, LinkedPortal);
	}
	else
	{
		// Fallback: no component present → old instant snap so nothing breaks.
		Player->SetActorLocation(ExitWorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
		if (LinkedPortal->GetVortex()) { LinkedPortal->GetVortex()->OnExitBloom(); }
		UCharacterMovementComponent* MC = Player->GetCharacterMovement();
		FVector V = MC->Velocity; V.X = 0.f; MC->Velocity = V;
	}

	BP_OnTeleportUsed(Player);

	if (Vortex) { Vortex->OnCollapse(); }   // source inhales
	if (LinkedPortal && LinkedPortal->Vortex) { LinkedPortal->Vortex->OnExitBloom(); } // exit exhales

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

	SetPortalCooldownVisual(true);
	if (LinkedPortal)
	{
		LinkedPortal->SetPortalCooldownVisual(true);
		LinkedPortal->BP_OnCooldownStarted();
	}
	BP_OnCooldownStarted();

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
	//SetPortalCooldownVisual(false);
	if (Vortex) { Vortex->OnCooldownComplete(); }
	BP_OnCooldownEnded();
}

void AkdShadowPortal::SetPortalCooldownVisual(bool bOnCooldown)
{
	//if (!PortalMesh) return;
	//const float TargetOpacity = bOnCooldown ? CooldownMeshOpacity : 1.0f;
	//PortalMesh->SetScalarParameterValueOnMaterials(OpacityParamName, TargetOpacity);

	if (Vortex) { Vortex->SetCooldownState(bOnCooldown); }
}

void AkdShadowPortal::OnPlayerArrived()
{
	if (Vortex) { Vortex->OnExitBloom(); }   // the exhale, timed to the reveal
}
