// Copyright ASKD Games


#include "UI/Widget/kdPauseMenuWidget.h"
#include "UI/HUD/kdHUD.h"
#include "GameInstance/kdGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

void UkdPauseMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (Btn_Resume)   Btn_Resume->OnClicked.AddDynamic(this, &UkdPauseMenuWidget::OnResumeClicked);
    if (Btn_Settings) Btn_Settings->OnClicked.AddDynamic(this, &UkdPauseMenuWidget::OnSettingsClicked);
    if (Btn_Restart)  Btn_Restart->OnClicked.AddDynamic(this, &UkdPauseMenuWidget::OnRestartClicked);
    if (Btn_MainMenu) Btn_MainMenu->OnClicked.AddDynamic(this, &UkdPauseMenuWidget::OnMainMenuClicked);
}

void UkdPauseMenuWidget::RefreshLivesDisplay(int32 RemainingLives)
{
    if (Txt_LivesLeft)
    {
       return Txt_LivesLeft->SetText(FText::FromString(FString::Printf(TEXT("Lives: %d"), RemainingLives)));
    }
}

void UkdPauseMenuWidget::OnResumeClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
        {
            HUD->HidePauseMenu();
        }
    }
}

void UkdPauseMenuWidget::OnSettingsClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
        {
            // SetVisibility(Collapsed) here is now redundant — HUD::ShowSettings
            // handles hiding the caller. Safe to remove this line.
            HUD->ShowSettings(true);
        }
    }
}

void UkdPauseMenuWidget::OnRestartClicked()
{
    if (UkdGameInstance* GI = UkdGameInstance::Get(GetWorld()))
    {
        GI->ReloadCurrentLevel();
    }
}

void UkdPauseMenuWidget::OnMainMenuClicked()
{
    if (UkdGameInstance* GI = UkdGameInstance::Get(GetWorld()))
    {
        GI->LoadMainMenu();
    }
}
