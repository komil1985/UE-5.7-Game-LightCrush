// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdHUDWidget.generated.h"

class UProgressBar;

/**
 * UkdHUDWidget
 *
 * Screen-space HUD view for the player's vital bars (Health + Stamina).
 *
 * PURE VIEW. It never reads gameplay attributes and never subscribes to the ASC.
 * All values are pushed in by UkdPlayerHUDComponent (single writer). Keeping the
 * subscription off the widget means the widget can be safely recreated across
 * level loads without ever disturbing the ASC delegate bindings that live on the
 * persistent component.
 *
 * Fixed on-screen placement ("like every other game") is authored in the WBP via
 * anchors + alignment. This class only exposes the bind points and setters.
 */
UCLASS()
class THEPERSPVIEW_API UkdHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Sets the health bar fill (clamped to [0,1]). */
    void SetHealthPercent(float InPercent01);

    /** Sets the stamina bar fill (clamped to [0,1]). */
    void SetStaminaPercent(float InPercent01);

    /** Optional exhausted flourish hook (e.g. flash the stamina bar to ExhaustRed). */
    void SetExhausted(bool bExhausted);

protected:
    virtual void NativeConstruct() override;

    /** WBP element names MUST match these identifiers exactly (BindWidget contract). */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> Bar_Health = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> Bar_Stamina = nullptr;

    /** Cached last-pushed values so a late NativeConstruct still shows the correct fill. */
    float PendingHealth01 = 1.f;
    float PendingStamina01 = 1.f;
    bool  bPendingExhausted = false;
};
