// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdGameOverWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdGameOverWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    virtual void NativeOnInitialized() override;

    /**
     * Populate all score / high-score labels and show the correct
     * buttons.  Call before AddToViewport.
     */
    UFUNCTION(BlueprintCallable, Category = "GameOver")
    void SetupGameOver(int32 FinalScore);

    /** Triggers the appear animation (calls BP_OnAppear). */
    UFUNCTION(BlueprintCallable, Category = "GameOver")
    void PlayAppearAnimation();

    // ── Blueprint hooks ───────────────────────────────────────────────────────

    UFUNCTION(BlueprintImplementableEvent, Category = "GameOver")
    void BP_OnAppear();

    UFUNCTION(BlueprintImplementableEvent, Category = "GameOver")
    void BP_OnNewHighScore();

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_GameOver;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_FinalScore;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_HighScore;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_NewHighScore;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_LivesLostLabel;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_Retry;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_MainMenu;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage>     Img_Background;

private:
    UFUNCTION() 
    void OnRetryClicked();
    
    UFUNCTION() 
    void OnMainMenuClicked();
};

// ─────────────────────────────────────────────────────────────────────────────
// UkdGameOverWidget — full-screen shown when the player's lives hit zero.
//
// Blueprint UMG bindings required:
//   Txt_GameOver          (TextBlock)  – "GAME OVER" main title
//   Txt_FinalScore        (TextBlock)  – score achieved this run
//   Txt_HighScore         (TextBlock)  – all-time best for this level
//   Txt_NewHighScore      (TextBlock)  – "★ NEW HIGH SCORE! ★"  (hidden by default)
//   Txt_LivesLostLabel    (TextBlock)  – static "Your shadow faded..."
//   Btn_Retry             (Button)     – reload current level
//   Btn_MainMenu          (Button)     – return to main menu
//   Img_Background        (Image)      – dark full-screen backdrop
//
// ANIMATION:
//   Add a UMG animation named "Appear" — fade/scale title in over ~0.8s.
//   Override BP_OnAppear to play it.
//
// DESIGN NOTE:
//   The "GAME OVER" header uses a glyph font for maximum atmosphere.
//   Use a typewriter effect on Txt_LivesLostLabel for dramatic pacing.
//   Both are achievable purely in Blueprint via the "Appear" animation or
//   a custom AnimateText BindWidget function.
// ─────────────────────────────────────────────────────────────────────────────