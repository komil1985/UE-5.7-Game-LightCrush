// Copyright ASKD Games


#include "World/kdKillVolume.h"
#include "Player/kdMyPlayer.h"
#include "GameMode/kdGameModeBase.h"
#include "Components/BoxComponent.h"


AkdKillVolume::AkdKillVolume()
{
	PrimaryActorTick.bCanEverTick = false;

    KillBox = CreateDefaultSubobject<UBoxComponent>(TEXT("KillBox"));
    SetRootComponent(KillBox);

    // Large default extent — designer scales this in the level
    KillBox->SetBoxExtent(FVector(2000.f, 2000.f, 100.f));
    KillBox->SetCollisionProfileName(TEXT("OverlapAll"));
    KillBox->SetGenerateOverlapEvents(true);

    // Hidden in-game but visible in editor via built-in box debug draw
    KillBox->SetHiddenInGame(true);
    KillBox->SetLineThickness(2.f);
}

#if WITH_EDITOR
void AkdKillVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    // Keep the box visible in editor so designers can resize it easily
    if (KillBox)
    {
        KillBox->ShapeColor = FColor(255, 0, 0);  // red in editor
    }
}
#endif

void AkdKillVolume::BeginPlay()
{
	Super::BeginPlay();

    KillBox->OnComponentBeginOverlap.AddDynamic(this, &AkdKillVolume::OnKillBoxOverlap);
	
}

void AkdKillVolume::OnKillBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
    if (!Player) return;

    if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        GM->HandlePlayerDeath();
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("KillVolume [%s]: Player fell out of bounds."), *GetName());
#endif
}


