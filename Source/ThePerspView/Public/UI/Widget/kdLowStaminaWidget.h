// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdLowStaminaWidget.generated.h"


class UTextBlock;

/**
 *  * World-space widget that flashes "LOW STAMINA!" above the player's head
 * whenever they press the Crush button while stamina is still depleted
 * (i.e. during the post-exhaustion regen delay window).
 *
 * Blueprint UMG binding required:
 *   Txt_LowStamina  (TextBlock)  — the warning label
 *
 * Optional Blueprint animation:
 *   Override BP_OnFlash() to play a "FlashIn" UMG animation
 *   (e.g. scale-punch + fade-out) on the TextBlock.
 *
 * SETUP:
 *   1. Create WBP_LowStaminaWarning inheriting from this class.
 *   2. Add a TextBlock named Txt_LowStamina — style it (bold, red-orange).
 *   3. Assign it to LowStaminaWidgetClass on BP_Player.
 */

UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdLowStaminaWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    /**
     * Shows the warning and restarts the auto-hide timer.
     * Safe to call on every press — timer is reset each time.
     */
    UFUNCTION(BlueprintCallable, Category = "Stamina | Warning")
    void FlashWarning();

    /**
     * How long the label stays visible before fading out automatically.
     * Each new press resets this window.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Stamina | Warning", meta = (ClampMin = "0.5", ClampMax = "5.0"))
    float FlashDuration = 1.5f;

    /**
     * Blueprint hook — play your flash animation here (scale-punch, fade-out, etc.).
     * Called every time FlashWarning() fires.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Stamina | Warning")
    void BP_OnFlash();

protected:
    // UMG binding — must exist in the Blueprint widget
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Txt_LowStamina;

private:
    FTimerHandle HideTimerHandle;

    /** Called by the timer after FlashDuration seconds. */
    void HideWarning();
};
