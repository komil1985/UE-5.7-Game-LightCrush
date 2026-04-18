// Copyright ASKD Games


#include "UI/Widget/kdLevelCompleteWidget.h"
#include "UI/HUD/kdHUD.h"
#include "GameInstance/kdGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

void UkdLevelCompleteWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (Btn_NextLevel) Btn_NextLevel->OnClicked.AddDynamic(this, &UkdLevelCompleteWidget::OnNextLevelClicked);
    if (Btn_Retry) Btn_Retry->OnClicked.AddDynamic(this, &UkdLevelCompleteWidget::OnRetryClicked);
    if (Btn_MainMenu)  Btn_MainMenu->OnClicked.AddDynamic(this, &UkdLevelCompleteWidget::OnMainMenuClicked);
}

void UkdLevelCompleteWidget::SetupResults(float CompletionTimeSeconds, int32 Score)
{
    UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());
    if (!GI) return;

    const int32 LevelIdx = GI->GetCurrentLevelIndex();

    // ── Time ──────────────────────────────────────────────────────────────────
    if (Txt_Time)
    {
        Txt_Time->SetText(FText::FromString(FormatTime(CompletionTimeSeconds)));
    }

    // ── Score ─────────────────────────────────────────────────────────────────
    if (Txt_Score)
    {
        Txt_Score->SetText(FText::AsNumber(Score));
    }

    // ── Best Time ─────────────────────────────────────────────────────────────
    if (Txt_BestTime)
    {
        const float BestTime = GI->GetBestTime(LevelIdx);
        const bool  bNewBest = (BestTime > 0.f && FMath::IsNearlyEqual(BestTime, CompletionTimeSeconds, 0.1f));

        Txt_BestTime->SetText(bNewBest
            ? FText::FromString(TEXT("★ NEW BEST! ★"))
            : FText::FromString(FString::Printf(TEXT("Best: %s"), *FormatTime(BestTime))));

        // Highlight colour for new best
        Txt_BestTime->SetColorAndOpacity(bNewBest
            ? FLinearColor(1.0f, 0.85f, 0.0f)   // Gold
            : FLinearColor(0.8f, 0.8f, 0.8f));   // Grey
    }

    // ── Best Score ───────────────────────────────────────────────────────────
    if (Txt_BestScore)
    {
        const int32 BestScore = GI->GetHighScore(LevelIdx);
        const bool  bNewHigh = (Score >= BestScore);

        Txt_BestScore->SetText(bNewHigh
            ? FText::FromString(TEXT("★ HIGH SCORE! ★"))
            : FText::AsNumber(BestScore));

        Txt_BestScore->SetColorAndOpacity(bNewHigh
            ? FLinearColor(1.0f, 0.85f, 0.0f)
            : FLinearColor(0.8f, 0.8f, 0.8f));
    }

    // ── Next Level button ─────────────────────────────────────────────────────
    if (Btn_NextLevel)
    {
        const bool bHasNext = GI->HasNextLevel();
        Btn_NextLevel->SetVisibility(bHasNext
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed);
    }
}

void UkdLevelCompleteWidget::PlayAppearAnimation()
{
    BP_OnAppear();
    // If you have a named "Appear" UMG animation, call PlayAnimation() here.
    // UMG animations can be played from Blueprint using the "Play Animation"
    // node tied to BP_OnAppear — keeping C++ decoupled from animation names.
}

void UkdLevelCompleteWidget::OnNextLevelClicked()
{
    if (UkdGameInstance* GI = UkdGameInstance::Get(GetWorld()))
    {
        GI->LoadNextLevel();
    }
}

void UkdLevelCompleteWidget::OnRetryClicked()
{
    if (UkdGameInstance* GI = UkdGameInstance::Get(GetWorld()))
    {
        GI->ReloadCurrentLevel();
    }
}

void UkdLevelCompleteWidget::OnMainMenuClicked()
{
    if (UkdGameInstance* GI = UkdGameInstance::Get(GetWorld()))
    {
        GI->LoadMainMenu();
    }
}

FString UkdLevelCompleteWidget::FormatTime(float TotalSeconds)
{
    if (TotalSeconds <= 0.f) return TEXT("--:--");

    const int32 Minutes = FMath::FloorToInt(TotalSeconds / 60.f);
    const int32 Seconds = FMath::FloorToInt(FMath::Fmod(TotalSeconds, 60.f));
    return FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
}
