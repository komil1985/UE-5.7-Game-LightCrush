// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveGame/kdSaveGame.h"
#include "kdSettingsWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;
class UCheckBox;
class UComboBoxString;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct()     override;

protected:
    // ── Audio ─────────────────────────────────────────────────────────────────
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider>    Slider_Master;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider>    Slider_Music;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider>    Slider_SFX;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Master;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Music;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_SFX;

    // ── Graphics ──────────────────────────────────────────────────────────────
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo_Quality;
    // Optional bind: only present in Blueprints that expose the resolution control.
    // Marked OptionalWidget so existing WBP_Settings assets that predate this
    // control still compile without the binding until you add the combo box.
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Resolution;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UCheckBox>       Check_Fullscreen;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UCheckBox>       Check_VSync;

    // ── Controls ──────────────────────────────────────────────────────────────
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider>    Slider_Sensitivity;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Sensitivity;

    // ── Navigation ────────────────────────────────────────────────────────────
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_Apply;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_Back;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_ResetDefaults;

private:
    // Pending settings (staged until Apply is clicked)
    FkdGameSettings PendingSettings;
    bool bHasUnsavedChanges = false;

    /**
     * Parallel to Combo_Resolution's option list: AvailableResolutions[i] is the
     * concrete FIntPoint behind option i. Keeping the raw values here means we
     * never have to parse a display label ("1920 x 1080  (16:9)") back into
     * pixels — GatherUIIntoSettings just indexes this array by selected index.
     */
    TArray<FIntPoint> AvailableResolutions;

    void LoadCurrentSettingsToUI();
    void GatherUIIntoSettings(FkdGameSettings& OutSettings) const;
    void UpdateVolumeLabel(UTextBlock* Label, float Value);

    /** Enumerate the monitor's supported fullscreen modes into Combo_Resolution. */
    void PopulateResolutionOptions();
    /** Select the combo entry matching Res, falling back gracefully if absent. */
    void SelectResolutionInUI(FIntPoint Res);

    UFUNCTION() void OnMasterSliderChanged(float Value);
    UFUNCTION() void OnMusicSliderChanged(float Value);
    UFUNCTION() void OnSFXSliderChanged(float Value);
    UFUNCTION() void OnSensitivityChanged(float Value);
    UFUNCTION() void OnQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnResolutionSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnFullscreenChanged(bool bIsChecked);
    UFUNCTION() void OnVSyncChanged(bool bIsChecked);
    UFUNCTION() void OnApplyClicked();
    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnResetDefaultsClicked();
};


// ─────────────────────────────────────────────────────────────────────────────
// UkdSettingsWidget — Audio, Graphics and Controls settings panel.
//
// This single widget is shared by BOTH the main menu and the pause menu; AkdHUD
// routes to it via ShowSettings(bFromPause) and returns to the correct caller.
// Any control added here therefore appears in both contexts automatically.
//
// Blueprint UMG bindings required:
//   Audio:
//     Slider_Master, Slider_Music, Slider_SFX
//     Txt_Master,    Txt_Music,    Txt_SFX    (percentage labels)
//   Graphics:
//     Combo_Quality       (Low / Medium / High / Epic)
//     Combo_Resolution    (ComboBoxString — auto-populated at runtime; optional)
//     Check_Fullscreen
//     Check_VSync
//   Controls:
//     Slider_Sensitivity
//     Txt_Sensitivity
//   Navigation:
//     Btn_Apply
//     Btn_Back
//     Btn_ResetDefaults
// ─────────────────────────────────────────────────────────────────────────────
