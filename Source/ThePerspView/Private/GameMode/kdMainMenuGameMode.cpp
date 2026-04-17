// Copyright ASKD Games


#include "GameMode/kdMainMenuGameMode.h"
#include "UI/HUD/kdHUD.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AkdMainMenuGameMode::AkdMainMenuGameMode()
{
	// No default pawn needed — the menu runs without a player character
	DefaultPawnClass = nullptr;
}

void AkdMainMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

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
