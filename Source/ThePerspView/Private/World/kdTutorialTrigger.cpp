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
    if (bFired && bOneShotPerLevel) return;
    if (!Cast<AkdMyPlayer>(OtherActor)) return;

    if (UkdTutorialSubsystem* Sub = UkdTutorialSubsystem::Get(this))
    {
        // RequestStep is idempotent: if OnBegin and the sweep both reach here,
        // the second call is ignored (already queued / active / seen).
        Sub->RequestStep(StepId, this);
        bFired = true;
    }
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
