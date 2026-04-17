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
    virtual void NativeConstruct()     override;

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_Play;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_Settings;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_Quit;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Version;

    // Button click handlers
    UFUNCTION() void OnPlayClicked();
    UFUNCTION() void OnSettingsClicked();
    UFUNCTION() void OnQuitClicked();
};

// ─────────────────────────────────────────────────────────────────────────────
// Blueprint UMG Setup:
//   Create a WBP_MainMenu widget. Add these named bindings:
//   • Btn_Play        (UButton)   – starts the first/next unlocked level
//   • Btn_Settings    (UButton)   – opens settings overlay
//   • Btn_Quit        (UButton)   – quits the application
//   • Txt_GameTitle   (UTextBlock)– game title
//   • Txt_Version     (UTextBlock)– auto-populated with build version
// ─────────────────────────────────────────────────────────────────────────────