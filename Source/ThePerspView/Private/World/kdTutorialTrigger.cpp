// Copyright ASKD Games

#include "World/kdTutorialTrigger.h"
#include "Subsystem/kdTutorialSubsystem.h"
#include "Player/kdMyPlayer.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"


AkdTutorialTrigger::AkdTutorialTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    SetRootComponent(TriggerBox);
    TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 150.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->SetGenerateOverlapEvents(true);
    TriggerBox->SetHiddenInGame(true);
    TriggerBox->ShapeColor = FColor(232, 184, 75);   // Sunmark, for editor legibility

    // Respawn anchor. Defaults to the box origin; drag it down onto the floor
    // in the level so recovered players land standing, not at capsule-centre height.
    RespawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RespawnPoint"));
    RespawnPoint->SetupAttachment(TriggerBox);
}

void AkdTutorialTrigger::BeginPlay()
{
    Super::BeginPlay();

    if (StepId.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("[kdTutorialTrigger] '%s' has no StepId."), *GetName());
    }

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AkdTutorialTrigger::OnBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AkdTutorialTrigger::OnEnd);

    // SPAWN-INSIDE FIX.
    // If the player spawns already overlapping this box, the initial overlap was
    // computed at component registration — before the delegate above was bound —
    // so BeginOverlap never fires to us. Defer one tick (the pawn may not exist or
    // be possessed on this exact frame), then sweep whoever is already inside.
    // Same next-tick deferral discipline the audio route uses for PostLoadMapWithWorld.
    GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { CheckInitialOverlap(); }));
}

void AkdTutorialTrigger::OnBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    HandlePawnEntered(OtherActor);
}

void AkdTutorialTrigger::OnEnd(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
    if (!Cast<AkdMyPlayer>(OtherActor)) return;

    if (UkdTutorialSubsystem* Sub = UkdTutorialSubsystem::Get(this))
    {
        Sub->NotifyTriggerExit(this);
        if (bCancelOnExit) Sub->CancelStep(StepId);
    }
}

void AkdTutorialTrigger::HandlePawnEntered(AActor* OtherActor)
{
    if (!Cast<AkdMyPlayer>(OtherActor)) return;

    UkdTutorialSubsystem* Sub = UkdTutorialSubsystem::Get(this);
    if (!Sub) return;

    // ALWAYS refresh the checkpoint — including re-touching a one-shot trigger,
    // so walking back through Room 1 makes Room 1 your recovery point again.
    Sub->SetCheckpoint(RespawnPoint->GetComponentLocation());

    // The lesson itself still respects one-shot rules.
    if (bFired && bOneShotPerLevel) return;

    Sub->RequestStep(StepId, this);
    bFired = true;
}

void AkdTutorialTrigger::CheckInitialOverlap()
{
    if (bFired && bOneShotPerLevel) return;

    // Force overlaps to be current before querying — guards against a stale set
    // if the pawn was still settling its transform on the prior frame.
    TriggerBox->UpdateOverlaps();

    TArray<AActor*> Overlapping;
    TriggerBox->GetOverlappingActors(Overlapping, AkdMyPlayer::StaticClass());

    for (AActor* Actor : Overlapping)
    {
        HandlePawnEntered(Actor);
        if (bFired && bOneShotPerLevel) break;   // one-shot: first player is enough
    }
}
