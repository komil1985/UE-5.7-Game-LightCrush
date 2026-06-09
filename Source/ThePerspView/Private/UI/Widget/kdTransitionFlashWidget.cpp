// Copyright ASKD Games


#include "UI/Widget/kdTransitionFlashWidget.h"                
#include "Blueprint/WidgetTree.h"        
#include "Components/Widget.h"          
#include "Components/Image.h"           
#include "UI/ColorLibrary/kdHUDColorLibrary.h"
#include "UI/ColorLibrary/kdThemeAccess.h"

void UkdTransitionFlashWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Start hidden — no flash until a transition fires.
    if (Img_Flash)
    {
        Img_Flash->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
    }
}

void UkdTransitionFlashWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
    Super::NativeTick(MyGeometry, DeltaTime);

    if (!Img_Flash) return;

    UkdColorTheme* Theme = UkdThemeAccess::GetColorTheme(this);
    if (!Theme) return;

    const float Alpha = UkdThemeAccess::GetBlendAlpha(this);

    // GetTransitionFlashColor already encodes the sin(α·π)*0.25 envelope —
    // zero at rest, peak at mid-blend. No extra gating needed.
    const FLinearColor FlashColor = UkdHUDColorLibrary::GetTransitionFlashColor(Theme, Alpha);
    Img_Flash->SetColorAndOpacity(FlashColor);

    const float ScreenW = MyGeometry.GetLocalSize().X;
    // Sweep travel: from -200 (off-screen left) to ScreenW + 200 (off-screen right)
    const float SweepX = FMath::Lerp(-200.f, ScreenW + 200.f, Alpha);

    if (UWidget* ShutterBar = WidgetTree->FindWidget<UImage>(TEXT("Img_ShutterBar")))
    {
        ShutterBar->SetRenderTranslation(FVector2D(SweepX, 0.f));
    }
}
