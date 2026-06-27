// Copyright ASKD Games

#include "World/kdLevelGoal.h"
#include "GameMode/kdGameModeBase.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"

AkdLevelGoal::AkdLevelGoal()
{
    PrimaryActorTick.bCanEverTick = false;

    GoalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalMesh"));
    SetRootComponent(GoalMesh);
    GoalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
    TriggerSphere->SetupAttachment(RootComponent);
    TriggerSphere->SetSphereRadius(150.f);
    TriggerSphere->SetCollisionProfileName(TEXT("OverlapAll"));
    TriggerSphere->SetGenerateOverlapEvents(true);

    IdleParticle = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("IdleParticle"));
    IdleParticle->SetupAttachment(RootComponent);
    IdleParticle->bAutoActivate = true;
}

void AkdLevelGoal::BeginPlay()
{
    Super::BeginPlay();

    TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AkdLevelGoal::OnTriggerBeginOverlap);
    TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AkdLevelGoal::OnTriggerEndOverlap);
}

void AkdLevelGoal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Never leave a dangling tag-event registration on a player's ASC.
    UnbindStateTracking();
    Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// Eligibility
// ─────────────────────────────────────────────────────────────────────────────

bool AkdLevelGoal::IsPlayerEligible(UAbilitySystemComponent* ASC) const
{
    if (!ASC) return false;

    const FkdGameplayTags& T = FkdGameplayTags::Get();
    const bool bInCrush = ASC->HasMatchingGameplayTag(T.State_CrushMode);

    switch (ReachMode)
    {
    case EkdGoalReachMode::CrushModeOnly:
        if (!bInCrush) return false;
        break;

    case EkdGoalReachMode::LightModeOnly:
        if (bInCrush) return false;
        break;

    case EkdGoalReachMode::EitherMode:
    default:
        break;  // any plane accepted
    }

    // InShadow only constrains anything while in Crush Mode; on the 3D plane the
    // tag can never be present, so requiring it there would be unsatisfiable.
    if (bRequireInShadow && bInCrush &&
        !ASC->HasMatchingGameplayTag(T.State_InShadow))
    {
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Completion
// ─────────────────────────────────────────────────────────────────────────────

void AkdLevelGoal::TryComplete()
{
    if (bTriggered) return;

    AkdMyPlayer* Player = TrackedPlayer.Get();
    if (!Player) return;

    if (!IsPlayerEligible(Player->GetAbilitySystemComponent())) return;

    bTriggered = true;

    // We've completed — no further state listening needed.
    UnbindStateTracking();

    // Stop idle particles — visual payoff before the screen fades.
    if (IdleParticle) IdleParticle->Deactivate();

    BP_OnGoalReached();     // Blueprint VFX + sound

    if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        GM->TriggerLevelComplete();
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("LevelGoal [%s]: Completed by %s (ReachMode=%d)"),
        *GetName(), *Player->GetName(), static_cast<int32>(ReachMode));
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Overlap
// ─────────────────────────────────────────────────────────────────────────────

void AkdLevelGoal::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bTriggered) return;

    AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
    if (!Player) return;

    TrackedPlayer = Player;

    // Immediate attempt — covers walking in already in an accepted state
    // (e.g. an Either / Light goal entered on the 3D plane).
    TryComplete();

    // Inside but not yet eligible (e.g. a Crush-only goal entered in 3D):
    // listen for the moment the player crushes / steps into shadow on the exit.
    if (!bTriggered)
    {
        BindStateTracking(Player);
    }
}

void AkdLevelGoal::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // Only respond to the player we're actually tracking leaving.
    if (TrackedPlayer.Get() != OtherActor) return;

    UnbindStateTracking();
    TrackedPlayer = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// State tracking (only active while a player is inside the trigger)
// ─────────────────────────────────────────────────────────────────────────────

void AkdLevelGoal::BindStateTracking(AkdMyPlayer* Player)
{
    UnbindStateTracking();   // never stack registrations

    if (!Player) return;

    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    if (!ASC) return;

    const FkdGameplayTags& T = FkdGameplayTags::Get();

    CrushTagHandle = ASC->RegisterGameplayTagEvent(
        T.State_CrushMode, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &AkdLevelGoal::OnTrackedStateChanged);

    ShadowTagHandle = ASC->RegisterGameplayTagEvent(
        T.State_InShadow, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &AkdLevelGoal::OnTrackedStateChanged);

    TrackedASC = ASC;
}

void AkdLevelGoal::UnbindStateTracking()
{
    if (UAbilitySystemComponent* ASC = TrackedASC.Get())
    {
        const FkdGameplayTags& T = FkdGameplayTags::Get();
        ASC->UnregisterGameplayTagEvent(CrushTagHandle, T.State_CrushMode, EGameplayTagEventType::NewOrRemoved);
        ASC->UnregisterGameplayTagEvent(ShadowTagHandle, T.State_InShadow, EGameplayTagEventType::NewOrRemoved);
    }

    CrushTagHandle.Reset();
    ShadowTagHandle.Reset();
    TrackedASC = nullptr;
}

void AkdLevelGoal::OnTrackedStateChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
    // A gating tag flipped while the player stands on the goal — re-evaluate.
    // TryComplete() is guarded by bTriggered, so this is safe to spam.
    TryComplete();
}