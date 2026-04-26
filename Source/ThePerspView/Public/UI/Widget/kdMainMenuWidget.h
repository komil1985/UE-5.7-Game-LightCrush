// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UkdGameInstance;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_NewGame;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_Continue;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_Settings;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_Quit;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Version;

    // Button click handlers
    UFUNCTION() void OnNewGameClicked();
    UFUNCTION() void OnContinueClicked();
    UFUNCTION() void OnSettingsClicked();
    UFUNCTION() void OnQuitClicked();
    UFUNCTION() void RefreshContinueButton();

private:
    /** Helper: determines the highest level the player has unlocked. */
    int32 GetHighestUnlockedLevelIndex() const;

    /** Helper: triggers the loading-screen-then-load sequence. */
    void TransitionToLevel(int32 LevelIndexBeforeLoadNext);

    UPROPERTY(EditDefaultsOnly, Category = "Time")
    float LoadingScreenDisplayTime = 5.0f;

};

// ─────────────────────────────────────────────────────────────────────────────
// UkdMainMenuWidget
//
// BLUEPRINT BINDINGS (rename Btn_Play to Btn_NewGame, add Btn_Continue):
//   Btn_NewGame   – starts fresh (resets save's CurrentLevelIndex to 0)
//   Btn_Continue  – resumes from highest unlocked level (disabled if no save)
//   Btn_Settings  – opens settings
//   Btn_Quit      – quits the application
//   Txt_Version   – auto-filled with project version
// ─────────────────────────────────────────────────────────────────────────────