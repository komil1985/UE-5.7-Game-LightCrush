// Copyright ASKD Games


#include "GameMode/kdMainMenuGameMode.h"
#include "UI/HUD/kdHUD.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Audio/kdAudioSubsystem.h"


AkdMainMenuGameMode::AkdMainMenuGameMode()
{
	// No default pawn needed — the menu runs without a player character
	DefaultPawnClass = nullptr;
}

void AkdMainMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    // BeginPlay owns music for the initial PIE launch. PostLoadMapWithWorld does NOT
    // fire in PIE for the first level (PIE world is created by duplication, never via
    // UEngine::LoadMap). OnPostLoadMap in GameInstance handles all subsequent
    // OpenLevel transitions. The same-track guard in RequestMusic makes the
    // double-call harmless if both happen to fire.
    if (UkdAudioSubsystem* Audio = UkdAudioSubsystem::Get(this))
    {
        Audio->RequestMenuMusic();
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC) return;

    // Show mouse cursor and enable UI-mode input
    PC->SetShowMouseCursor(true);
    PC->SetInputMode(FInputModeUIOnly());

    // Tell the HUD to display the Main Menu
    if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
    {
        HUD->ShowMainMenu();
    }
}
