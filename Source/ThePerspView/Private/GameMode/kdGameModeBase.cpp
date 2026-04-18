// Copyright ASKD Games


#include "GameMode/kdGameModeBase.h"
#include "World/kdCheckpoint.h"
#include "Player/kdMyPlayer.h"
#include "GameInstance/kdGameInstance.h"
#include "UI/HUD/kdHUD.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "GameplayTags/kdGameplayTags.h"

AkdGameModeBase::AkdGameModeBase()
{
	PrimaryActorTick.bCanEverTick = true;
	RemainingLives = StartingLives;
}

void AkdGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    RemainingLives = StartingLives;
    CurrentScore = 0;
    ElapsedTime = 0.f;
    bLevelComplete = false;
    bGamePaused = false;

    // Stamp session start so the GameInstance knows when we started
    if (UkdGameInstance* GI = UkdGameInstance::Get(this))
    {
        GI->LevelStartTime = GetWorld()->GetTimeSeconds();
        GI->SessionScore = 0;
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("AkdGameModeBase: BeginPlay — Lives=%d"), RemainingLives);
#endif
}

void AkdGameModeBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bLevelComplete && !bGamePaused)
    {
        ElapsedTime += DeltaSeconds;
    }
}

void AkdGameModeBase::TriggerLevelComplete()
{
    if (bLevelComplete) return;  // guard against double-fire
    bLevelComplete = true;

    // Freeze the game timer but keep rendering
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.0f);
    // Re-enable input briefly so the UI is interactive
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);

    // Compute time bonus (higher score for faster completion)
    const float TimeBonus = FMath::Max(0.f, 300.f - ElapsedTime) * 10.f;
    const int32 FinalScore = CurrentScore + FMath::RoundToInt(TimeBonus);

    // Persist result
    UkdGameInstance* GI = UkdGameInstance::Get(this);
    if (GI)
    {
        GI->SessionScore = FinalScore;
        const float BestPrev = GI->GetBestTime(GI->GetCurrentLevelIndex());
        const bool  bNewBest = (BestPrev <= 0.f || ElapsedTime < BestPrev);
        GI->RecordLevelComplete(GI->GetCurrentLevelIndex(), ElapsedTime, FinalScore);
        OnLevelComplete.Broadcast(ElapsedTime, FinalScore, bNewBest);
    }
    else
    {
        OnLevelComplete.Broadcast(ElapsedTime, FinalScore, false);
    }

    // Tell the HUD to show the Level Complete screen
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
        {
            HUD->ShowLevelCompleteScreen(ElapsedTime, FinalScore);
        }
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("LevelComplete — Time=%.1fs  Score=%d"), ElapsedTime, FinalScore);
#endif
}

void AkdGameModeBase::RegisterCheckpoint(AkdCheckpoint* NewCheckpoint)
{
    if (NewCheckpoint && NewCheckpoint != ActiveCheckpoint)
    {
        ActiveCheckpoint = NewCheckpoint;
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("GameMode: Checkpoint registered — %s"), *NewCheckpoint->GetName());
#endif
    }
}

void AkdGameModeBase::HandlePlayerDeath()
{
    if (bLevelComplete) return;

    RemainingLives = FMath::Max(0, RemainingLives - 1);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("PlayerDeath — Lives remaining: %d"), RemainingLives);
#endif

    if (RemainingLives <= 0)
    {
        // Game Over — reload level or go back to main menu
        // Small delay so death feedback is visible
        FTimerHandle GameOverHandle;
        GetWorldTimerManager().SetTimer(GameOverHandle, [this]()
            {
                if (UkdGameInstance* GI = UkdGameInstance::Get(this))
                {
                    GI->ReloadCurrentLevel();
                }
            }, 2.0f, false);
        return;
    }

    // Tell HUD to show death feedback
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
        {
            HUD->ShowDeathFeedback(RemainingLives);
        }
    }

    // Delayed respawn
    GetWorldTimerManager().SetTimer(
        RespawnTimerHandle, this, &AkdGameModeBase::PerformRespawn, RespawnDelay, false);
}

void AkdGameModeBase::AddScore(int32 Points)
{
    CurrentScore += Points;

    if (UkdGameInstance* GI = UkdGameInstance::Get(this))
    {
        GI->SessionScore = CurrentScore;
    }
}

void AkdGameModeBase::PauseGame()
{
    if (bGamePaused || bLevelComplete) return;
    bGamePaused = true;
    UGameplayStatics::SetGamePaused(GetWorld(), true);
}

void AkdGameModeBase::UnpauseGame()
{
    if (!bGamePaused) return;
    bGamePaused = false;
    UGameplayStatics::SetGamePaused(GetWorld(), false);
}

void AkdGameModeBase::RestartPlayer(AController* NewPlayer)
{
    // Override to prevent default UE restart which respawns at a PlayerStart
    // ignoring our checkpoint system. We handle respawn entirely above.
}

void AkdGameModeBase::PerformRespawn()
{
    AkdMyPlayer* Player = GetCachedPlayer();
    if (!Player) return;

    FVector RespawnLocation;
    FRotator RespawnRotation = FRotator::ZeroRotator;

    if (ActiveCheckpoint)
    {
        RespawnLocation = ActiveCheckpoint->GetRespawnTransform().GetLocation();
        RespawnRotation = ActiveCheckpoint->GetRespawnTransform().GetRotation().Rotator();
    }
    else
    {
        // Fallback: use the first PlayerStart in the level
        AActor* Start = FindPlayerStart(Player->GetController());
        if (Start)
        {
            RespawnLocation = Start->GetActorLocation();
            RespawnRotation = Start->GetActorRotation();
        }
    }

    Player->SetActorLocation(RespawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Player->SetActorRotation(RespawnRotation);

    // Restore full stamina
    if (UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent())
    {
        // Remove any exhausted/crush-mode tags left over from death
        const FkdGameplayTags& StateTags = FkdGameplayTags::Get();
        ASC->RemoveLooseGameplayTag(StateTags.State_Exhausted);
        ASC->RemoveLooseGameplayTag(StateTags.State_CrushMode);
        ASC->RemoveLooseGameplayTag(StateTags.State_InShadow);
    }

    OnPlayerRespawned.Broadcast(RemainingLives);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("PerformRespawn: Teleported to %s"), *RespawnLocation.ToString());
#endif
}

AkdMyPlayer* AkdGameModeBase::GetCachedPlayer() const
{
    return Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
}
