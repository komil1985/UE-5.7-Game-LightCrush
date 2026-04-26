// Copyright ASKD Games


#include "UI/Widget/kdMainMenuWidget.h"
#include "UI/HUD/kdHUD.h"
#include "GameInstance/kdGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"


void UkdMainMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (Btn_NewGame) Btn_NewGame->OnClicked.AddDynamic(this, &UkdMainMenuWidget::OnNewGameClicked);
    if (Btn_Continue) Btn_Continue->OnClicked.AddDynamic(this, &UkdMainMenuWidget::OnContinueClicked);
    if (Btn_Settings) Btn_Settings->OnClicked.AddDynamic(this, &UkdMainMenuWidget::OnSettingsClicked);
    if (Btn_Quit) Btn_Quit->OnClicked.AddDynamic(this, &UkdMainMenuWidget::OnQuitClicked);
}

void UkdMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

     //Populate version label from project version string in DefaultGame.ini
    if (Txt_Version)
    {
        const FString VersionStr = FString::Printf(TEXT("v%d"), *FApp::GetBuildVersion());
        Txt_Version->SetText(FText::FromString(VersionStr));
    }

    RefreshContinueButton();
}

void UkdMainMenuWidget::OnNewGameClicked()
{
    //UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());
    //if (!GI) return;

    //APlayerController* PC = GetOwningPlayer();
    //if (!PC) return;

    //AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD());
    //// Show loading screen before level open so the transition feels intentional
    //if (HUD)
    //{
    //    HUD->ShowLoadingScreen();
    //}

    //// Level index 1 = first playable level (index 0 is the main menu itself)
    //GI->SetCurrentLevelIndex(1);

    //FTimerHandle LoadHandle;
    //GetWorld()->GetTimerManager().SetTimer(LoadHandle, [GI](){GI->LoadNextLevel();}, LoadingScreenDisplayTime, false);

    // New Game always starts at Level 1 (index 1). LoadNextLevel will increment
    // CurrentLevelIndex (0 → 1), so we set it to 0 here.
    TransitionToLevel(0);
}

void UkdMainMenuWidget::OnContinueClicked()
{
    UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());
    if (!GI) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    // ── Use LastPlayedLevelIndex directly ────────────────────────────────────
    // This is saved to disk in RecordLevelComplete so it survives restarts.
    // No more guessing from unlock flags — we know exactly where to resume.
    const int32 ResumeIndex = GI->GetLastPlayedLevelIndex();

    if (!GI->LevelOrder.IsValidIndex(ResumeIndex))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Continue: ResumeIndex %d is out of range. Defaulting to Level 1."),
            ResumeIndex);
        GI->SetCurrentLevelIndex(1);
    }
    else
    {
        GI->SetCurrentLevelIndex(ResumeIndex);
    }

    if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
    {
        HUD->ShowLoadingScreen();
    }

    constexpr float LoadDelay = 5.0f;
    FTimerHandle LoadHandle;
    GetWorld()->GetTimerManager().SetTimer(LoadHandle, [GI]()
        {
            GI->LoadCurrentLevel();
        }, LoadDelay, false);
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
    UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UkdMainMenuWidget::RefreshContinueButton()
{
    if (!Btn_Continue) return;

    UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());

    // Continue is only meaningful if the player has reached at least Level 2.
    // LastPlayedLevelIndex starts at 1 for new saves, so >= 2 means real progress.
    const bool bHasProgress = GI && (GI->GetLastPlayedLevelIndex() >= 2);

    Btn_Continue->SetIsEnabled(bHasProgress);
    Btn_Continue->SetVisibility(bHasProgress ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

int32 UkdMainMenuWidget::GetHighestUnlockedLevelIndex() const
{
    UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());
    if (!GI) return 1;

    // ── FIX: Get the real number of levels so we never overshoot the array ──
    // Levels[0] = Main Menu, Levels[1] = Level 1, Levels[2] = Level 2, ...
    // LastPlayableIndex is the index of the last actual playable level.
    const int32 TotalLevels = GI->GetTotalLevelCount();   // e.g. 4
    const int32 LastPlayableIndex = TotalLevels - 1;           // e.g. 3

    int32 Highest = 1;  // Level 1 is always available

    for (int32 i = 2; i <= LastPlayableIndex; ++i)
    {
        if (GI->IsLevelUnlocked(i))
        {
            Highest = i;
        }
        else
        {
            break;  // First locked level — stop searching
        }
    }
    return Highest;
}

void UkdMainMenuWidget::TransitionToLevel(int32 LevelIndexBeforeLoadNext)
{
    UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());
    if (!GI) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
    {
        HUD->ShowLoadingScreen();
    }

    GI->SetCurrentLevelIndex(LevelIndexBeforeLoadNext);

    constexpr float LoadDelay = 5.0f;
    FTimerHandle LoadHandle;
    GetWorld()->GetTimerManager().SetTimer(LoadHandle, [GI]()
        {
            GI->LoadNextLevel();  // increments CurrentLevelIndex then opens level
        }, LoadDelay, false);
}
