// Copyright ASKD Games


#include "UI/Widget/kdMainMenuWidget.h"
#include "UI/HUD/kdHUD.h"
#include "GameInstance/kdGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"


void UkdMainMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (Btn_Play)     Btn_Play->OnClicked.AddDynamic(this, &UkdMainMenuWidget::OnPlayClicked);
    if (Btn_Settings) Btn_Settings->OnClicked.AddDynamic(this, &UkdMainMenuWidget::OnSettingsClicked);
    if (Btn_Quit)     Btn_Quit->OnClicked.AddDynamic(this, &UkdMainMenuWidget::OnQuitClicked);
}

void UkdMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Populate version label from project version string in DefaultGame.ini
    //if (Txt_Version)
    //{
    //    const FString VersionStr = FString::Printf(TEXT("v%s"), *FApp::GetBuildVersion());
    //    Txt_Version->SetText(FText::FromString(VersionStr));
    //}
}

void UkdMainMenuWidget::OnPlayClicked()
{
    UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());
    if (!GI) return;

    // Show loading screen before level open so the transition feels intentional
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
        {
            HUD->ShowLoadingScreen();
        }
    }

    // Level index 1 = first playable level (index 0 is the main menu itself)
    GI->SetCurrentLevelIndex(1);
    GI->LoadNextLevel();
}

void UkdMainMenuWidget::OnSettingsClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
        {
            HUD->ShowSettings(false);
        }
    }
}

void UkdMainMenuWidget::OnQuitClicked()
{
    // Save before quitting
    if (UkdGameInstance* GI = UkdGameInstance::Get(GetWorld()))
    {
        GI->SaveProgress();
    }
    UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(),
        EQuitPreference::Quit, false);
}
