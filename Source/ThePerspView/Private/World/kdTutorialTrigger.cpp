// Copyright ASKD Games

#include "World/kdTutorialTrigger.h"
#include "Subsystem/kdTutorialSubsystem.h"
#include "Player/kdMyPlayer.h"
#include "Components/BoxComponent.h"


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
}

void AkdTutorialTrigger::OnBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
    int32, bool, const FHitResult&)
{
    if (bFired && bOneShotPerLevel) return;
    if (!Cast<AkdMyPlayer>(OtherActor)) return;

    if (UkdTutorialSubsystem* Sub = UkdTutorialSubsystem::Get(this))
    {
        Sub->RequestStep(StepId, this);
        bFired = true;
    }
}

void AkdTutorialTrigger::OnEnd(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
    if (!Cast<AkdMyPlayer>(OtherActor)) return;

    if (UkdTutorialSubsystem* Sub = UkdTutorialSubsystem::Get(this))
    {
        Sub->NotifyTriggerExit(this);          // satisfies ExitVolume steps
        if (bCancelOnExit) Sub->CancelStep(StepId);
    }
}