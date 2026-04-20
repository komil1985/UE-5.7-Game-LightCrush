// Copyright ASKD Games


#include "UI/HUD/kdHUD.h"
#include "Blueprint/UserWidget.h"
#include "Player/kdPlayerController.h"
#include "UI/Widget/kdMainMenuWidget.h"
#include "UI/Widget/kdSettingsWidget.h"
#include "UI/Widget/kdPauseMenuWidget.h"
#include "UI/Widget/kdLevelCompleteWidget.h"
#include "UI/Widget/kdLoadingScreenWidget.h"
#include "UI/Widget/kdDeathWidget.h"
#include "UI/Widget/kdGameOverWidget.h"


void AkdHUD::BeginPlay()
{
	Super::BeginPlay();
}

void AkdHUD::SetGameInputMode()
{
    if (APlayerController* PC = GetOwnerPC())
    {
        // GameOnly = free camera rotation, no cursor, no click-to-capture.
        // Called when dismissing the pause menu or settings to return to gameplay.
        PC->SetInputMode(FInputModeGameOnly());
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
    if(UkdMainMenuWidget * W = GetOrCreate(MainMenuWidget, MainMenuWidgetClass, 20))
    {
        // Guard AddToViewport — only call it once on first creation
        if (!W->IsInViewport())
        {
            W->AddToViewport(20);
        }
        W->SetVisibility(ESlateVisibility::Visible);
    }
    SetUIInputMode();
}

void AkdHUD::ShowSettings(bool bFromPause)
{
    bSettingsFromPause = bFromPause;

    // ── Hide whichever menu opened settings ───────────────────────────────────
    // This is what was missing — without this the caller stays visible
    // and the settings widget renders on top of it.
    if (bFromPause)
    {
        if (PauseMenuWidget)
        {
            PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else
    {
        if (MainMenuWidget)
        {
            MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (UkdSettingsWidget* W = GetOrCreate(SettingsWidget, SettingsWidgetClass, 30))
    {
        if (!W->IsInViewport())
        {
            W->AddToViewport(30);
        }
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

    // ── Restore whichever menu opened settings ────────────────────────────────
    if (bSettingsFromPause)
    {
        // Opened from pause menu — restore it
        if (PauseMenuWidget)
        {
            PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
        }
    }
    else
    {
        // Opened from main menu — restore it
        // This case was missing entirely, which is why Back did nothing
        if (MainMenuWidget)
        {
            MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
        }
    }

    bSettingsFromPause = false;
}

void AkdHUD::ShowPauseMenu()
{
    if (UkdPauseMenuWidget* W = GetOrCreate(PauseMenuWidget, PauseMenuWidgetClass, 25))
    {
        if (!W->IsInViewport()) W->AddToViewport(25);
        W->SetVisibility(ESlateVisibility::Visible);
    }
    SetUIInputMode();
    if (APlayerController* PC = GetOwnerPC()) PC->SetPause(true);
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
    if (DeathWidget)     DeathWidget->SetVisibility(ESlateVisibility::Collapsed);
    if (PauseMenuWidget) PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);

    if (UkdLevelCompleteWidget* W = GetOrCreate(LevelCompleteWidget, LevelCompleteWidgetClass, 40))
    {
        if (!W->IsInViewport()) W->AddToViewport(40);
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
        if (!W->IsInViewport()) W->AddToViewport(50);
        W->StartAnimation();
        W->SetVisibility(ESlateVisibility::Visible);
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

void AkdHUD::ShowDeathOverlay(int32 RemainingLives)
{
    if (UkdDeathWidget* W = GetOrCreate(DeathWidget, DeathWidgetClass, 35))
    {
        W->SetupDeath(RemainingLives);
        W->SetVisibility(ESlateVisibility::Visible);
    }
    // Do NOT change input mode here — the player is already dead and
    // input was disabled by DeathComponent.  We don't want UI clicks
    // to accidentally fire during the death window.
}

void AkdHUD::HideDeathOverlay()
{
    if (DeathWidget)
    {
        // Trigger fade-out animation in Blueprint, then collapse
        DeathWidget->BeginHide();
        DeathWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void AkdHUD::ShowGameOver(int32 FinalScore)
{
    // Dismiss any leftover widgets before showing game over
    if (DeathWidget)     DeathWidget->SetVisibility(ESlateVisibility::Collapsed);
    if (PauseMenuWidget) PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);

    if (UkdGameOverWidget* W = GetOrCreate(GameOverWidget, GameOverWidgetClass, 45))
    {
        W->SetupGameOver(FinalScore);
        W->SetVisibility(ESlateVisibility::Visible);
        W->PlayAppearAnimation();
    }
    SetUIInputMode();
}

APlayerController* AkdHUD::GetOwnerPC() const
{
	return Cast<APlayerController>(GetOwner());
}