// Copyright ASKD Games


#include "UI/Widget/kdGameOverWidget.h"
#include "GameInstance/kdGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "GameFramework/PlayerController.h"


void UkdGameOverWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (Btn_Retry)    Btn_Retry->OnClicked.AddDynamic(this, &UkdGameOverWidget::OnRetryClicked);
    if (Btn_MainMenu) Btn_MainMenu->OnClicked.AddDynamic(this, &UkdGameOverWidget::OnMainMenuClicked);

    // Hide the "new high score" banner by default — revealed by SetupGameOver
    if (Txt_NewHighScore)
    {
        Txt_NewHighScore->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UkdGameOverWidget::SetupGameOver(int32 FinalScore)
{
    UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());

    // ── Flavour label ─────────────────────────────────────────────────────────
    if (Txt_LivesLostLabel)
    {
        Txt_LivesLostLabel->SetText(
            FText::FromString(TEXT("Your shadow faded into the light...")));
    }

    // ── GAME OVER title ───────────────────────────────────────────────────────
    if (Txt_GameOver)
    {
        Txt_GameOver->SetText(FText::FromString(TEXT("GAME OVER")));
        Txt_GameOver->SetColorAndOpacity(FLinearColor(0.85f, 0.1f, 0.1f, 1.f));
    }

    // ── Final score ───────────────────────────────────────────────────────────
    if (Txt_FinalScore)
    {
        Txt_FinalScore->SetText(
            FText::FromString(FString::Printf(TEXT("Score: %d"), FinalScore)));
    }

    // ── High score ────────────────────────────────────────────────────────────
    if (GI)
    {
        const int32 LevelIdx = GI->GetCurrentLevelIndex();
        const int32 BestScore = GI->GetHighScore(LevelIdx);
        const bool  bNewBest = (FinalScore > BestScore);

        if (Txt_HighScore)
        {
            // Show the previous best (before this run may overwrite it)
            const int32 DisplayBest = bNewBest ? FinalScore : BestScore;
            Txt_HighScore->SetText(
                FText::FromString(
                    FString::Printf(TEXT("Best: %d"), DisplayBest)));
        }

        if (bNewBest)
        {
            if (Txt_NewHighScore)
            {
                Txt_NewHighScore->SetVisibility(ESlateVisibility::Visible);
                Txt_NewHighScore->SetText(
                    FText::FromString(TEXT("★  NEW HIGH SCORE!  ★")));
                Txt_NewHighScore->SetColorAndOpacity(
                    FLinearColor(1.0f, 0.85f, 0.0f, 1.f));    // gold
            }
            BP_OnNewHighScore();    // hook for particle burst / sound in Blueprint
        }
    }
    else
    {
        // No GameInstance (shouldn't happen in a real session — guard anyway)
        if (Txt_HighScore)
        {
            Txt_HighScore->SetText(FText::FromString(TEXT("---")));
        }
    }
}

void UkdGameOverWidget::PlayAppearAnimation()
{
    BP_OnAppear();
    // Blueprint subclass overrides BP_OnAppear and calls
    // PlayAnimation(Appear) on the "Appear" UMG animation.
}

void UkdGameOverWidget::OnRetryClicked()
{
    if (UkdGameInstance* GI = UkdGameInstance::Get(GetWorld()))
    {
        GI->ReloadCurrentLevel();
    }
}

void UkdGameOverWidget::OnMainMenuClicked()
{
    if (UkdGameInstance* GI = UkdGameInstance::Get(GetWorld()))
    {
        GI->LoadMainMenu();
    }
}
