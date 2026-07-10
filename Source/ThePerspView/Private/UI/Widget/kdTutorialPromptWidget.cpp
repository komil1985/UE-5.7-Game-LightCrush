// Copyright ASKD Games

#include "UI/Widget/kdTutorialPromptWidget.h"
#include "UI/ColorLibrary/kdHUDColorRamp.h"   
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/World.h"

UkdTutorialPromptWidget::UkdTutorialPromptWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    HeadlineColor = kdHeliograph::Sunmark;
    BodyColor = kdHeliograph::Lumen;
    KeyCapColor = kdHeliograph::PaleIon;
}

void UkdTutorialPromptWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Txt_Headline) Txt_Headline->SetColorAndOpacity(FSlateColor(HeadlineColor));
    if (Txt_Body)     Txt_Body->SetColorAndOpacity(FSlateColor(BodyColor));
    if (Txt_Key)      Txt_Key->SetColorAndOpacity(FSlateColor(KeyCapColor));

    HideImmediate();
}

void UkdTutorialPromptWidget::HideImmediate()
{
    Phase = EPhase::Hidden;
    PhaseTime = 0.f;
    LastRevealed = -1;
    SetRenderOpacity(0.f);
    SetRenderScale(FVector2D(1.f, 1.f));
    SetVisibility(ESlateVisibility::Collapsed);
}

void UkdTutorialPromptWidget::ShowStep(const FText& InHeadline, const FText& InBody, const FText& InKeyGlyph)
{
    FullBody = InBody.ToString();
    LastRevealed = -1;

    if (Txt_Headline) Txt_Headline->SetText(InHeadline);
    if (Txt_Body)     Txt_Body->SetText(FText::GetEmpty());

    const bool bHasGlyph = !InKeyGlyph.IsEmpty();
    if (Txt_Key)    Txt_Key->SetText(InKeyGlyph);
    if (Border_Key) Border_Key->SetVisibility(bHasGlyph ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed);

    Phase = EPhase::Developing;
    PhaseTime = 0.f;

    // HitTestInvisible: the prompt must never eat a click or block gameplay input.
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetRenderOpacity(0.f);
    SetRenderScale(FVector2D(1.02f, 1.02f));
}

void UkdTutorialPromptWidget::PlayDismiss()
{
    if (Phase == EPhase::Hidden || Phase == EPhase::Dismissing) return;
    Phase = EPhase::Dismissing;
    PhaseTime = 0.f;
}

void UkdTutorialPromptWidget::ApplyBodyReveal(int32 VisibleChars, bool bShowCursor)
{
    if (!Txt_Body || VisibleChars == LastRevealed) return;
    LastRevealed = VisibleChars;

    FString Out = FullBody.Left(VisibleChars);
    if (bShowCursor) Out.AppendChar(TEXT('\u258D'));   // ▍ block cursor
    Txt_Body->SetText(FText::FromString(Out));
}

void UkdTutorialPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (Phase == EPhase::Hidden || Phase == EPhase::Held) return;

    // Slate keeps ticking widgets while the game is paused. The plate must not
    // develop behind the pause menu.
    const UWorld* W = GetWorld();
    if (W && W->IsPaused()) return;

    PhaseTime += InDeltaTime;

    switch (Phase)
    {
    case EPhase::Developing:
    {
        const float A = (DevelopSeconds > KINDA_SMALL_NUMBER)
            ? FMath::Clamp(PhaseTime / DevelopSeconds, 0.f, 1.f) : 1.f;

        // Ease-out: fast exposure, slow settle — the shape of a real fixer bath.
        const float Eased = 1.f - FMath::Pow(1.f - A, 3.f);
        SetRenderOpacity(Eased);
        SetRenderScale(FVector2D(FMath::Lerp(1.02f, 1.f, Eased)));

        if (A >= 1.f) { Phase = EPhase::Typing; PhaseTime = 0.f; }
        break;
    }
    case EPhase::Typing:
    {
        const int32 Total = FullBody.Len();
        const int32 Visible = FMath::Min(Total, FMath::FloorToInt(PhaseTime * CharsPerSecond));
        ApplyBodyReveal(Visible, /*bShowCursor*/ Visible < Total);

        if (Visible >= Total)
        {
            ApplyBodyReveal(Total, false);
            Phase = EPhase::Held;
            PhaseTime = 0.f;
        }
        break;
    }
    case EPhase::Dismissing:
    {
        const float A = FMath::Clamp(PhaseTime / DismissSeconds, 0.f, 1.f);
        SetRenderOpacity(1.f - A);
        if (A >= 1.f) HideImmediate();
        break;
    }
    default: break;
    }
}