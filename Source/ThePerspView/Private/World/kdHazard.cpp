// Copyright ASKD Games


#include "World/kdHazard.h"
#include "Player/kdMyPlayer.h"
#include "GameMode/kdGameModeBase.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"


AkdHazard::AkdHazard()
{
	PrimaryActorTick.bCanEverTick = false;

    HazardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HazardMesh"));
    SetRootComponent(HazardMesh);
    HazardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    HazardVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("HazardVolume"));
    HazardVolume->SetupAttachment(RootComponent);
    HazardVolume->SetBoxExtent(FVector(50.f, 50.f, 50.f));
    HazardVolume->SetCollisionProfileName(TEXT("OverlapAll"));
    HazardVolume->SetGenerateOverlapEvents(true);
}

void AkdHazard::BeginPlay()
{
	Super::BeginPlay();
	
    HazardVolume->OnComponentBeginOverlap.AddDynamic(this, &AkdHazard::OnHazardVolumeOverlap);
}

void AkdHazard::OnHazardVolumeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bOnCooldown) return;

    AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
    if (!Player) return;

    if (!IsLethalToPlayer(Player)) return;

    // ── Arm immunity window first to block re-entry ───────────────────────────
    bOnCooldown = true;
    GetWorldTimerManager().SetTimer(
        ImmunityTimerHandle, this,
        &AkdHazard::ResetCooldown,
        ImmunityDuration, false);

    // ── Audio / VFX ───────────────────────────────────────────────────────────
    if (HazardSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, HazardSound, GetActorLocation());
    }
    BP_OnPlayerKilled();

    // ── Kill ──────────────────────────────────────────────────────────────────
    if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        GM->HandlePlayerDeath();
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("HazardActor [%s]: Killed player."), *GetName());
#endif
}

bool AkdHazard::IsLethalToPlayer(AkdMyPlayer* Player) const
{
    if (Behavior == EHazardBehavior::Always) return true;

    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    if (!ASC) return true;  // no ASC = can't check tags = default to lethal

    const bool bInCrushMode =
        ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode);

    if (Behavior == EHazardBehavior::CrushModeOnly)  return  bInCrushMode;
    if (Behavior == EHazardBehavior::NormalModeOnly) return !bInCrushMode;

    return true;
}

void AkdHazard::ResetCooldown()
{
    bOnCooldown = false;
}


