// Copyright ASKD Games


#include "World/kdCheckpoint.h"
#include "GameMode/kdGameModeBase.h"
#include "Player/kdMyPlayer.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"

AkdCheckpoint::AkdCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

    CheckpointMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CheckpointMesh"));
    SetRootComponent(CheckpointMesh);

    TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
    TriggerSphere->SetupAttachment(RootComponent);
    TriggerSphere->SetSphereRadius(120.f);
    TriggerSphere->SetCollisionProfileName(TEXT("OverlapAll"));
    TriggerSphere->SetGenerateOverlapEvents(true);

    ActivationParticle = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ActivationParticle"));
    ActivationParticle->SetupAttachment(RootComponent);
    ActivationParticle->bAutoActivate = false;  // fires only on first activation
}

void AkdCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	
    TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AkdCheckpoint::OnTriggerBeginOverlap);
}

FTransform AkdCheckpoint::GetRespawnTransform() const
{
    FVector Location = GetActorLocation();
    Location.Z += RespawnOffsetZ;
    return FTransform(GetActorRotation(), Location);
}

void AkdCheckpoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Only activate for the player — never enemies or physics objects
    if (!Cast<AkdMyPlayer>(OtherActor)) return;

    if (!bActivated)
    {
        Activate();
    }

    // Always tell the GameMode this is now the active respawn point,
    // even if the checkpoint was already activated before.
    if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        GM->RegisterCheckpoint(this);
    }
}

void AkdCheckpoint::Activate()
{
    bActivated = true;

    // Swap to the "lit" material
    if (ActiveMaterial)
    {
        CheckpointMesh->SetMaterial(0, ActiveMaterial);
    }

    // Play activation particle burst
    if (ActivationParticle)
    {
        ActivationParticle->Activate(true);
    }

    // Play activation sound
    if (ActivationSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ActivationSound, GetActorLocation());
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("Checkpoint [%s] Activated."), *GetName());
#endif
}


