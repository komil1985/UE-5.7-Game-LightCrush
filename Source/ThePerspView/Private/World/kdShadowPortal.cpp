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

	// Portal starts hidden/inactive. BeginPlay resolves the correct initial
	// state from the player's shadow tag (or forces it visible when the portal
	// is configured to always show).
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AkdShadowPortal::BeginPlay()
{
	Super::BeginPlay();

	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AkdShadowPortal::OnTriggerBeginOverlap);

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Player) return;

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (!ASC) return;

	// Vortex needs the ASC + mesh regardless of reveal policy.
	Vortex->InitializeForMesh(PortalMesh, ASC);

	if (!bRevealOnlyInShadow)
	{
		// Always visible/interactive; the teleport path still checks State.Shaded.
		SetPortalActive(true);
		return;
	}

	// Reveal-in-shadow: track State.Shaded so the portal appears/disappears in
	// sync with shadow — in BOTH 2D and 3D — with no polling.
	ASC->RegisterGameplayTagEvent(
		FkdGameplayTags::Get().State_Shaded,
		EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AkdShadowPortal::OnShadedTagChanged);

	// Sync immediately in case the player already stands in shadow (e.g. portal
	// placed where the level begins occluded). SetPortalActive drives the Vortex.
	const bool bAlreadyShaded =
		ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_Shaded);
	SetPortalActive(bAlreadyShaded);
}

void AkdShadowPortal::OnShadedTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// NewCount > 0  → player just became shaded → reveal portal
	// NewCount == 0 → player left shadow          → hide portal
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
	UE_LOG(LogTemp, Log, TEXT("ShadowPortal [%s]: SetPortalActive=%d"), *GetName(), bActive);
#endif
}

void AkdShadowPortal::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bCanTeleport || !LinkedPortal) return;

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
	if (!Player) return;

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (!ASC) return;

	const FkdGameplayTags& StateTags = FkdGameplayTags::Get();

	// ── Gate: shadow-only, in BOTH 2D and 3D ───────────────────────────────────
	// State.Shaded is the single authoritative "occluded from all lights" signal
	// maintained by UkdCrushStateComponent regardless of dimension.
	if (!ASC->HasMatchingGameplayTag(StateTags.State_Shaded)) return;

	// Never teleport mid-crush-transition or while dead — both would fight other
	// systems that are momentarily driving the pawn's transform/physics.
	if (ASC->HasMatchingGameplayTag(StateTags.State_Transitioning)) return;
	if (ASC->HasMatchingGameplayTag(StateTags.State_Dead)) return;

	// ── Teleport ──────────────────────────────────────────────────────────────
	// Place the player at the linked portal's location offset by ExitOffset
	// (in the linked portal's local space, so the exit direction is always correct).
	const FVector ExitWorldLocation = LinkedPortal->GetActorLocation() + LinkedPortal->GetActorRotation().RotateVector(LinkedPortal->ExitOffset);

	// Hand the smooth move to the player. It fades, moves under cover, applies
	// mode-aware exit velocity, and calls LinkedPortal->OnPlayerArrived() at the
	// covered moment for the exhale.
	if (UkdPortalTeleportComponent* TC = Player->FindComponentByClass<UkdPortalTeleportComponent>())
	{
		if (TC->IsTeleporting()) { return; }        // guard double-triggers
		TC->BeginTeleport(ExitWorldLocation, LinkedPortal);
	}
	else
	{
		// Fallback: no component present → old instant snap so nothing breaks.
		// Velocity is left untouched here; the polished path (TC) owns the
		// mode-aware collapse-axis handling.
		Player->SetActorLocation(ExitWorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
		if (LinkedPortal->GetVortex()) { LinkedPortal->GetVortex()->OnExitBloom(); }
	}

	BP_OnTeleportUsed(Player);

	if (Vortex) { Vortex->OnCollapse(); }   // source inhales
	if (LinkedPortal && LinkedPortal->Vortex) { LinkedPortal->Vortex->OnExitBloom(); } // exit exhales

	// NOTE: the old redundant `CarriedVelocity.X = 0` block was removed here — it
	// fought the teleport component (which re-applies velocity under cover) and
	// hardcoded the 2D collapse axis. Velocity is now owned solely by the TC.

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
