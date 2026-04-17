// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdLevelCompleteWidget.generated.h"

class UButton;
class UTextBlock;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdLevelCompleteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;

    /** Called by HUD immediately before making visible. */
    UFUNCTION(BlueprintCallable, Category = "Results")
    void SetupResults(float CompletionTimeSeconds, int32 Score);

    /** Triggers the "Appear" UMG animation if one exists. */
    UFUNCTION(BlueprintCallable, Category = "Results")
    void PlayAppearAnimation();

    /** Blueprint-implementable hook for the appear animation. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Results")
    void BP_OnAppear();

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Time;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Score;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_BestTime;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_BestScore;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_NextLevel;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_Retry;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_MainMenu;

    UFUNCTION() void OnNextLevelClicked();
    UFUNCTION() void OnRetryClicked();
    UFUNCTION() void OnMainMenuClicked();

private:
    static FString FormatTime(float TotalSeconds);
	
};


// ─────────────────────────────────────────────────────────────────────────────
// UkdLevelCompleteWidget
//
// Blueprint UMG bindings required:
//   Txt_Time          – formatted completion time  e.g. "01:23"
//   Txt_Score         – final score number
//   Txt_BestTime      – personal best or "NEW BEST!" label
//   Txt_HighScore     – personal best score or "NEW BEST!" label
//   Btn_NextLevel     – load next level (hidden on last level)
//   Btn_Retry         – reload current level
//   Btn_MainMenu      – return to main menu
//
// ANIMATION NOTE:
//   Add a UMG animation named "Appear" in Blueprint for slide-in effect.
//   PlayAppearAnimation() calls PlayAnimation("Appear") if it exists.
// ─────────────────────────────────────────────────────────────────────────────