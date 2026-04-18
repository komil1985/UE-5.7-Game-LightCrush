// Copyright ASKD Games


#include "UI/Widget/kdDeathWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"


void UkdDeathWidget::NativeConstruct()
{
    Super::NativeConstruct();

    DotTimer = 0.f;
    DotCount = 0;
}

void UkdDeathWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
    Super::NativeTick(MyGeometry, DeltaTime);
    UpdateRespawnDots(DeltaTime);
}

void UkdDeathWidget::SetupDeath(int32 RemainingLives)
{
    DotTimer = 0.f;
    DotCount = 0;

    // ── "YOU DIED" header ─────────────────────────────────────────────────────
    if (Txt_YouDied)
    {
        Txt_YouDied->SetText(FText::FromString(TEXT("YOU DIED")));
        // Saturated red — visually impactful but not eye-burning
        Txt_YouDied->SetColorAndOpacity(FLinearColor(0.9f, 0.1f, 0.1f, 1.f));
    }

    // ── Lives remaining ───────────────────────────────────────────────────────
    if (Txt_LivesRemaining)
    {
        FString LivesText;
        if (RemainingLives <= 0)
        {
            LivesText = TEXT("No Lives Remaining");
        }
        else if (RemainingLives == 1)
        {
            LivesText = TEXT("Last Life!");
            Txt_LivesRemaining->SetColorAndOpacity(FLinearColor(1.f, 0.4f, 0.0f, 1.f)); // orange warning
        }
        else
        {
            LivesText = FString::Printf(TEXT("%d Lives Remaining"), RemainingLives);
            Txt_LivesRemaining->SetColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.f));
        }
        Txt_LivesRemaining->SetText(FText::FromString(LivesText));
    }

    // ── Respawning hint — starts blank, dots animate in Tick ─────────────────
    if (Txt_RespawnHint)
    {
        Txt_RespawnHint->SetText(FText::FromString(TEXT("Respawning")));
    }

    // ── Trigger Blueprint fade-in animation ───────────────────────────────────
    BP_OnShow();
}

void UkdDeathWidget::BeginHide()
{
    // Stop animating dots so they don't flicker during fade-out
    DotTimer = 0.f;
    BP_OnHide();
}

void UkdDeathWidget::UpdateRespawnDots(float DeltaTime)
{
    if (!Txt_RespawnHint) return;

    DotTimer += DeltaTime;
    if (DotTimer >= HintDotInterval)
    {
        DotTimer -= HintDotInterval;
        DotCount = (DotCount + 1) % 4;    // cycles 0,1,2,3

        // Build "Respawning", "Respawning.", "Respawning..", "Respawning..."
        FString Dots;
        for (int32 i = 0; i < DotCount; ++i) Dots += TEXT(".");
        Txt_RespawnHint->SetText(FText::FromString(FString::Printf(TEXT("Respawning%s"), *Dots)));
    }
}
