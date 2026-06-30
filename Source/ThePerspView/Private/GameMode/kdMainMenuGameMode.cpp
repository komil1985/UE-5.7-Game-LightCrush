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

    // Start menu music as early as possible in the level's life so there's no
    // audible gap before the first widget paints. The subsystem is a
    // GameInstanceSubsystem, so it's already Initialize()'d and BankA is
    // already resolved by the time any level's BeginPlay runs — safe to call here.
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
