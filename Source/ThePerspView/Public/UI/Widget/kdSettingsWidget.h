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

    void LoadCurrentSettingsToUI();
    void GatherUIIntoSettings(FkdGameSettings& OutSettings) const;
    void UpdateVolumeLabel(UTextBlock* Label, float Value);

    UFUNCTION() void OnMasterSliderChanged(float Value);
    UFUNCTION() void OnMusicSliderChanged(float Value);
    UFUNCTION() void OnSFXSliderChanged(float Value);
    UFUNCTION() void OnSensitivityChanged(float Value);
    UFUNCTION() void OnQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnFullscreenChanged(bool bIsChecked);
    UFUNCTION() void OnVSyncChanged(bool bIsChecked);
    UFUNCTION() void OnApplyClicked();
    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnResetDefaultsClicked();
};


// ─────────────────────────────────────────────────────────────────────────────
// UkdSettingsWidget — Audio, Graphics and Controls settings panel.
//
// Blueprint UMG bindings required:
//   Audio:
//     Slider_Master, Slider_Music, Slider_SFX
//     Txt_Master,    Txt_Music,    Txt_SFX    (percentage labels)
//   Graphics:
//     Combo_Quality       (Low / Medium / High / Epic)
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