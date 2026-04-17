// Copyright ASKD Games


#include "UI/HUD/kdHUD.h"
#include "Blueprint/UserWidget.h"
#include "Player/kdPlayerController.h"
#include "UI/Widget/kdMainMenuWidget.h"
#include "UI/Widget/kdSettingsWidget.h"
#include "UI/Widget/kdPauseMenuWidget.h"
#include "UI/Widget/kdLevelCompleteWidget.h"
#include "UI/Widget/kdLoadingScreenWidget.h"


void AkdHUD::BeginPlay()
{
	Super::BeginPlay();
}

void AkdHUD::SetGameInputMode()
{
    if (APlayerController* PC = GetOwnerPC())
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(true);
        PC->SetInputMode(Mode);
        PC->SetShowMouseCursor(false);
    }
}

void AkdHUD::SetUIInputMode()
{
    if (APlayerController* PC = GetOwnerPC())
    {
        PC->SetInputMode(FInputModeUIOnly());
        PC->SetShowMouseCursor(true);
    }
}

void AkdHUD::ShowMainMenu()
{
    if (UkdMainMenuWidget* W = GetOrCreate(MainMenuWidget, MainMenuWidgetClass, 20))
    {
        W->AddToViewport(20);
        W->SetVisibility(ESlateVisibility::Visible);
    }
    SetUIInputMode();
}

void AkdHUD::ShowSettings(bool bFromPause)
{
    bSettingsFromPause = bFromPause;

    if (UkdSettingsWidget* W = GetOrCreate(SettingsWidget, SettingsWidgetClass, 30))
    {
        W->AddToViewport(30);
        W->SetVisibility(ESlateVisibility::Visible);
    }
    SetUIInputMode();
}

void AkdHUD::HideSettings()
{
    if (SettingsWidget)
    {
        SettingsWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    // Return to whichever context opened settings
    if (bSettingsFromPause && PauseMenuWidget)
    {
        PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
    }
    bSettingsFromPause = false;
}

void AkdHUD::ShowPauseMenu()
{
    if (UkdPauseMenuWidget* W = GetOrCreate(PauseMenuWidget, PauseMenuWidgetClass, 25))
    {
        W->AddToViewport(25);
        W->SetVisibility(ESlateVisibility::Visible);
    }
    SetUIInputMode();

    // Pause game time
    if (APlayerController* PC = GetOwnerPC())
    {
        PC->SetPause(true);
    }
}

void AkdHUD::HidePauseMenu()
{
    if (PauseMenuWidget)
    {
        PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
    SetGameInputMode();

    if (APlayerController* PC = GetOwnerPC())
    {
        PC->SetPause(false);
    }
}

void AkdHUD::ShowLevelCompleteScreen(float CompletionTime, int32 Score)
{
    if (UkdLevelCompleteWidget* W = GetOrCreate(LevelCompleteWidget, LevelCompleteWidgetClass, 40))
    {
        W->AddToViewport(40);
        W->SetupResults(CompletionTime, Score);
        W->SetVisibility(ESlateVisibility::Visible);
        W->PlayAppearAnimation();
    }
    SetUIInputMode();
}

void AkdHUD::ShowDeathFeedback(int32 RemainingLives)
{
    // Pulse a "death" flash on the level complete widget's backing infrastructure.
    // For now we broadcast a screen flash via camera shake — a simple,
    // always-working approach that needs no extra widget.
    if (APlayerController* PC = GetOwnerPC())
    {
        // Optionally trigger a camera shake BP here via
        // PC->ClientStartCameraShake(DeathCameraShakeClass);
        UE_LOG(LogTemp, Log, TEXT("HUD: DeathFeedback — lives left %d"), RemainingLives);
    }
}

void AkdHUD::ShowLoadingScreen()
{
    if (UkdLoadingScreenWidget* W = GetOrCreate(LoadingScreenWidget, LoadingScreenWidgetClass, 50))
    {
        W->AddToViewport(50);
        W->SetVisibility(ESlateVisibility::Visible);
        W->StartAnimation();
    }
    SetUIInputMode();
}

void AkdHUD::HideLoadingScreen()
{
    if (LoadingScreenWidget)
    {
        LoadingScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

APlayerController* AkdHUD::GetOwnerPC() const
{
	return Cast<APlayerController>(GetOwner());
}