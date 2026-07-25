// Copyright ASKD Games


#include "UI/Widget/kdCrushRegisterWidget.h"
#include "UI/ColorLibrary/kdHUDColorRamp.h"  
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void UkdCrushRegisterWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Heliograph defaults. Overridable per-WBP in the Details panel.
    //   Seeking = PaleIon     — a latent image, cool and unresolved.
    //   Locked  = GoldLeaf    — the exposure has taken.
    //   Denied  = EmberTrace  — the same warning hue the rest of the HUD uses.
    if (SeekingColor.IsAlmostBlack()) { SeekingColor = kdHeliograph::PaleIon; }
    if (LockedColor.IsAlmostBlack()) { LockedColor = kdHeliograph::GoldLeaf; }
    if (DeniedColor.IsAlmostBlack()) { DeniedColor = kdHeliograph::EmberTrace; }

    if (Label_PosX.IsEmpty()) { Label_PosX = NSLOCTEXT("kdRegister", "AxisN", "N"); }
    if (Label_NegX.IsEmpty()) { Label_NegX = NSLOCTEXT("kdRegister", "AxisS", "S"); }
    if (Label_PosY.IsEmpty()) { Label_PosY = NSLOCTEXT("kdRegister", "AxisE", "E"); }
    if (Label_NegY.IsEmpty()) { Label_NegY = NSLOCTEXT("kdRegister", "AxisW", "W"); }

    // Never intercept input; the register is decoration over a gameplay camera.
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetRenderOpacity(0.f);
}

void UkdCrushRegisterWidget::NativeDestruct()
{
    BindTo(nullptr);   // symmetric unbind — the component may outlive the widget
    Super::NativeDestruct();
}

void UkdCrushRegisterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    UkdCrushAlignmentComponent* Comp = ResolveAlignment();

    // No component (menu level, dead pawn, mid-respawn) → fade out and idle.
    const float TargetRegister = Comp ? Comp->GetRegister01() : 0.f;

    SmoothedRegister01 = FMath::FInterpTo(
        SmoothedRegister01, TargetRegister, InDeltaTime, VisualInterpSpeed);

    // ── Decay the one-shot cues ─────────────────────────────────────────────
    LockPunchTimer = FMath::Max(0.f, LockPunchTimer - InDeltaTime);
    DeniedFlashTimer = FMath::Max(0.f, DeniedFlashTimer - InDeltaTime);

    const float DeniedAlpha = (DeniedFlashDuration > 0.f)
        ? FMath::Clamp(DeniedFlashTimer / DeniedFlashDuration, 0.f, 1.f) : 0.f;

    // ── Opacity ─────────────────────────────────────────────────────────────
    // The denial flash forces the widget visible even at register 0 — that is
    // the whole point of it: the refusal must be SEEN, not inferred.
    const float BaseOpacity = (SmoothedRegister01 <= FadeInFloor) ? 0.f : SmoothedRegister01;
    const float Opacity = FMath::Max(BaseOpacity, DeniedAlpha);
    SetRenderOpacity(Opacity);

    if (Opacity <= KINDA_SMALL_NUMBER)
    {
        return;   // fully faded — skip all per-element work
    }

    // ── Colour ──────────────────────────────────────────────────────────────
    FLinearColor Tint = FMath::Lerp(SeekingColor, LockedColor, SmoothedRegister01);
    if (DeniedAlpha > 0.f)
    {
        Tint = FMath::Lerp(Tint, DeniedColor, DeniedAlpha);
    }
    Tint.A = 1.f;

    // ── Split-prism convergence ─────────────────────────────────────────────
    // Separation reaches exactly zero at register 1, so "the marks have met" and
    // "the press will succeed" are the same visual event. That equivalence is
    // what makes the readout teachable in one attempt.
    const float Separation = MaxMarkSeparation * (1.f - SmoothedRegister01);

    if (IsValid(Img_MarkLeft))
    {
        Img_MarkLeft->SetRenderTranslation(FVector2D(-Separation, 0.f));
        Img_MarkLeft->SetColorAndOpacity(Tint);
    }
    if (IsValid(Img_MarkRight))
    {
        Img_MarkRight->SetRenderTranslation(FVector2D(Separation, 0.f));
        Img_MarkRight->SetColorAndOpacity(Tint);
    }

    // ── Frame + lock punch ──────────────────────────────────────────────────
    if (IsValid(Img_Frame))
    {
        float Scale = 1.f;
        if (LockPunchTimer > 0.f && LockPunchDuration > 0.f)
        {
            // Ease-out: full punch at the instant of lock, settling to 1.
            const float T = FMath::Clamp(LockPunchTimer / LockPunchDuration, 0.f, 1.f);
            Scale += LockPunchScale * T * T;
        }
        Img_Frame->SetRenderScale(FVector2D(Scale, Scale));
        Img_Frame->SetColorAndOpacity(Tint);
    }

    // ── Axis label ──────────────────────────────────────────────────────────
    if (IsValid(Txt_Axis))
    {
        Txt_Axis->SetText(AxisLabelFor(Comp ? Comp->GetPredictedDirection() : EkdCrushDirection::None));
        Txt_Axis->SetColorAndOpacity(FSlateColor(Tint));
    }
}

// =============================================================================
// Events
// =============================================================================

void UkdCrushRegisterWidget::HandleRegisterStateChanged(
    EkdRegisterState NewState, EkdCrushDirection /*PredictedDirection*/)
{
    if (NewState == EkdRegisterState::Locked)
    {
        LockPunchTimer = LockPunchDuration;
    }
}

void UkdCrushRegisterWidget::HandleIntentResolved(bool bSucceeded)
{
    if (!bSucceeded)
    {
        DeniedFlashTimer = DeniedFlashDuration;
    }
}

// =============================================================================
// Resolution / binding
// =============================================================================

UkdCrushAlignmentComponent* UkdCrushRegisterWidget::ResolveAlignment()
{
    if (Alignment.IsValid())
    {
        return Alignment.Get();
    }

    // Re-resolve every frame while null: the pawn is replaced on respawn, so a
    // one-shot cache would silently go dead after the first death.
    APlayerController* PC = GetOwningPlayer();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    UkdCrushAlignmentComponent* Found =
        Pawn ? Pawn->FindComponentByClass<UkdCrushAlignmentComponent>() : nullptr;

    if (Found)
    {
        BindTo(Found);
    }
    return Found;
}

void UkdCrushRegisterWidget::BindTo(UkdCrushAlignmentComponent* NewComp)
{
    if (UkdCrushAlignmentComponent* Old = Alignment.Get())
    {
        Old->OnRegisterStateChanged.RemoveDynamic(this, &UkdCrushRegisterWidget::HandleRegisterStateChanged);
        Old->OnCrushIntentResolved.RemoveDynamic(this, &UkdCrushRegisterWidget::HandleIntentResolved);
    }

    Alignment = NewComp;

    if (NewComp)
    {
        // AddDynamic is safe here: this widget is created once and lives in the
        // viewport, unlike UWidgetComponent inner widgets which are destroyed
        // and recreated on visibility changes (and silently lose their bindings).
        NewComp->OnRegisterStateChanged.AddDynamic(this, &UkdCrushRegisterWidget::HandleRegisterStateChanged);
        NewComp->OnCrushIntentResolved.AddDynamic(this, &UkdCrushRegisterWidget::HandleIntentResolved);
    }
}

FText UkdCrushRegisterWidget::AxisLabelFor(EkdCrushDirection Direction) const
{
    switch (Direction)
    {
    case EkdCrushDirection::PosX: return Label_PosX;
    case EkdCrushDirection::NegX: return Label_NegX;
    case EkdCrushDirection::PosY: return Label_PosY;
    case EkdCrushDirection::NegY: return Label_NegY;
    default:                      return FText::GetEmpty();
    }
}
