// Copyright ASKD Games

#include "World/kdRespawnVolume.h"
#include "Subsystem/kdTutorialSubsystem.h"
#include "Player/kdMyPlayer.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"


AkdRespawnVolume::AkdRespawnVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    SetRootComponent(TriggerBox);

    // Wide and flat by default — a catch-plane, not a room trigger.
    TriggerBox->SetBoxExtent(FVector(5000.f, 5000.f, 100.f));

    // Explicit, minimal collision: overlaps pawns only, blocks nothing, never
    // casts shadow. Cleaner than leaning on a named profile.
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECC_WorldStatic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);
    TriggerBox->SetHiddenInGame(true);
    TriggerBox->SetCastShadow(false);
    TriggerBox->ShapeColor = FColor(232, 64, 64);   // ExhaustRed — "death" plane
}

void AkdRespawnVolume::BeginPlay()
{
    Super::BeginPlay();
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AkdRespawnVolume::OnBegin);
}

void AkdRespawnVolume::OnBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
    int32, bool, const FHitResult&)
{
    if (AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor))
    {
        RespawnPlayer(Player);
    }
}

void AkdRespawnVolume::RespawnPlayer(AkdMyPlayer* Player)
{
    // Debounce on REAL time: a teleport can leave a one-frame residual overlap,
    // and physics may report several overlap sub-steps for one fall. Real time is
    // correct here because it's immune to any game-time dilation the CMC applies.
    const float Now = GetWorld()->GetRealTimeSeconds();
    if (Now - LastRespawnTime < RespawnCooldown) return;

    UkdTutorialSubsystem* Sub = UkdTutorialSubsystem::Get(this);
    FVector Target;
    if (!Sub || !Sub->GetCheckpoint(Target))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[kdRespawnVolume] No checkpoint available — player not recovered."));
        return;
    }
    LastRespawnTime = Now;

    // Kill momentum so the fall velocity doesn't carry into the checkpoint. This
    // zeroes Velocity + Acceleration + pending input WITHOUT changing the movement
    // mode — important, because forcing MOVE_Walking would break the custom
    // CMOVE_Shadow2D mode if the player fell while crushed.
    if (UCharacterMovementComponent* CMC = Player->GetCharacterMovement())
    {
        CMC->StopMovementImmediately();
    }

    // If the player fell while crushed, restore 3D through the real ability path
    // BEFORE teleporting — so the CMC exits CMOVE_Shadow2D cleanly and the
    // checkpoint (a 3D-authored spot) receives a properly 3D player.
    Player->ForceExitCrush();

    // sweep=false so we don't collide through geometry on the way; TeleportPhysics
    // so animation/physics treat it as a hard discontinuity, not interpolated motion.
    Player->SetActorLocation(
        Target + FVector(0.f, 0.f, RespawnVerticalOffset),
        /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("[kdRespawnVolume] Recovered player to %s"), *Target.ToString());
#endif
}