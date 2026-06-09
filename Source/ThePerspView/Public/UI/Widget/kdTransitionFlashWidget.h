// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdTransitionFlashWidget.generated.h"

class UImage;
class UkdColorTheme;
class UkdWorldColorDriver;

/**
 * Full-screen edge-flash that fires every time the world driver blends.
 *
 * Mechanism: every frame, reads `WorldColorDriver::GetBlendAlpha()` and feeds
 * it through `UkdHUDColorLibrary::GetTransitionFlashColor`. That function
 * peaks at sin(α·π)·0.25 (i.e. brightest at mid-blend, zero at both ends) so
 * the widget is invisible at rest and self-rate-limits.
 *
 * No event subscriptions. Pure pull-based — robust against initialisation
 * order, doesn't require explicit Show/Hide calls.
 */
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdTransitionFlashWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

protected:
    /**
     * Full-screen Image bound by name. In the WBP, drop a UImage anchored
     * Fill (all four corners stretched to viewport) and rename it Img_Flash.
     * Brush should be a radial gradient (bright center → transparent edge)
     * OR a solid white; the C++ multiplies in the colour every tick.
     */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Img_Flash;
};
