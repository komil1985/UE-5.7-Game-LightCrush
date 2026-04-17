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
}

void AkdLevelGoal::BP_OnGoalReached()
{
}

void AkdLevelGoal::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bTriggered) return;

    AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
    if (!Player) return;

    // Tag gate
    if (bRequireCrushMode || bRequireInShadow)
    {
        UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
        if (!ASC) return;

        const FkdGameplayTags& Tags = FkdGameplayTags::Get();

        if (bRequireCrushMode && !ASC->HasMatchingGameplayTag(Tags.State_CrushMode)) return;
        if (bRequireInShadow && !ASC->HasMatchingGameplayTag(Tags.State_InShadow))  return;
    }

    bTriggered = true;

    // Stop idle particles — visual payoff before the screen fades
    if (IdleParticle) IdleParticle->Deactivate();

    BP_OnGoalReached();     // fires Blueprint VFX + sound

    if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        GM->TriggerLevelComplete();
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("LevelGoal [%s]: Triggered by %s"), *GetName(), *Player->GetName());
#endif
}
