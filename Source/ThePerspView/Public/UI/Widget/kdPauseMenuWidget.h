// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdPauseMenuWidget.generated.h"

class UButton;
class UTextBlock;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    virtual void NativeOnInitialized() override;

    /** Called by GameMode/HUD to refresh live data when the widget is shown. */
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void RefreshLivesDisplay(int32 RemainingLives);

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_Resume;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_Settings;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_Restart;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_MainMenu;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_LivesLeft;

    UFUNCTION() void OnResumeClicked();
    UFUNCTION() void OnSettingsClicked();
    UFUNCTION() void OnRestartClicked();
    UFUNCTION() void OnMainMenuClicked();
};


// ─────────────────────────────────────────────────────────────────────────────
// UkdPauseMenuWidget
//
// Blueprint UMG bindings required:
//   Btn_Resume      – return to gameplay
//   Btn_Settings    – open settings (bFromPause = true)
//   Btn_Restart     – reload current level
//   Btn_MainMenu    – return to main menu
//   Txt_LivesLeft   – displays remaining lives
// ─────────────────────────────────────────────────────────────────────────────