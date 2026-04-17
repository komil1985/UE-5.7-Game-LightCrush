// Copyright ASKD Games


#include "UI/Widget/kdSettingsWidget.h"
#include "UI/HUD/kdHUD.h"
#include "GameInstance/kdGameInstance.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "GameFramework/PlayerController.h"



void UkdSettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // Audio
    if (Slider_Master) Slider_Master->OnValueChanged.AddDynamic(this, &UkdSettingsWidget::OnMasterSliderChanged);
    if (Slider_Music)  Slider_Music->OnValueChanged.AddDynamic(this, &UkdSettingsWidget::OnMusicSliderChanged);
    if (Slider_SFX)    Slider_SFX->OnValueChanged.AddDynamic(this, &UkdSettingsWidget::OnSFXSliderChanged);

    // Graphics
    if (Combo_Quality)    Combo_Quality->OnSelectionChanged.AddDynamic(this, &UkdSettingsWidget::OnQualitySelectionChanged);
    if (Check_Fullscreen) Check_Fullscreen->OnCheckStateChanged.AddDynamic(this, &UkdSettingsWidget::OnFullscreenChanged);
    if (Check_VSync)      Check_VSync->OnCheckStateChanged.AddDynamic(this, &UkdSettingsWidget::OnVSyncChanged);

    // Controls
    if (Slider_Sensitivity) Slider_Sensitivity->OnValueChanged.AddDynamic(this, &UkdSettingsWidget::OnSensitivityChanged);

    // Nav buttons
    if (Btn_Apply)         Btn_Apply->OnClicked.AddDynamic(this, &UkdSettingsWidget::OnApplyClicked);
    if (Btn_Back)          Btn_Back->OnClicked.AddDynamic(this, &UkdSettingsWidget::OnBackClicked);
    if (Btn_ResetDefaults) Btn_ResetDefaults->OnClicked.AddDynamic(this, &UkdSettingsWidget::OnResetDefaultsClicked);

    // Quality preset options
    if (Combo_Quality)
    {
        Combo_Quality->AddOption(TEXT("Low"));
        Combo_Quality->AddOption(TEXT("Medium"));
        Combo_Quality->AddOption(TEXT("High"));
        Combo_Quality->AddOption(TEXT("Epic"));
    }
}

void UkdSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    LoadCurrentSettingsToUI();
}

void UkdSettingsWidget::LoadCurrentSettingsToUI()
{
    UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());
    if (!GI) return;

    const FkdGameSettings& S = GI->GetGameSettings();

    if (Slider_Master) Slider_Master->SetValue(S.MasterVolume);
    if (Slider_Music)  Slider_Music->SetValue(S.MusicVolume);
    if (Slider_SFX)    Slider_SFX->SetValue(S.SFXVolume);

    UpdateVolumeLabel(Txt_Master, S.MasterVolume);
    UpdateVolumeLabel(Txt_Music, S.MusicVolume);
    UpdateVolumeLabel(Txt_SFX, S.SFXVolume);

    if (Combo_Quality)
    {
        const TArray<FString> Options = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
        if (Options.IsValidIndex(S.QualityPreset))
        {
            Combo_Quality->SetSelectedOption(Options[S.QualityPreset]);
        }
    }

    if (Check_Fullscreen) Check_Fullscreen->SetIsChecked(S.bFullscreen);
    if (Check_VSync)      Check_VSync->SetIsChecked(S.bVSync);

    if (Slider_Sensitivity) Slider_Sensitivity->SetValue(S.MouseSensitivity);
    UpdateVolumeLabel(Txt_Sensitivity, S.MouseSensitivity);

    bHasUnsavedChanges = false;
}

void UkdSettingsWidget::GatherUIIntoSettings(FkdGameSettings& OutSettings) const
{
    if (Slider_Master) OutSettings.MasterVolume = Slider_Master->GetValue();
    if (Slider_Music)  OutSettings.MusicVolume = Slider_Music->GetValue();
    if (Slider_SFX)    OutSettings.SFXVolume = Slider_SFX->GetValue();

    if (Combo_Quality)
    {
        const TArray<FString> Options = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
        const FString Selected = Combo_Quality->GetSelectedOption();
        OutSettings.QualityPreset = Options.IndexOfByKey(Selected);
        if (OutSettings.QualityPreset == INDEX_NONE) OutSettings.QualityPreset = 2;
    }

    if (Check_Fullscreen) OutSettings.bFullscreen = Check_Fullscreen->IsChecked();
    if (Check_VSync)      OutSettings.bVSync = Check_VSync->IsChecked();
    if (Slider_Sensitivity) OutSettings.MouseSensitivity = Slider_Sensitivity->GetValue();
}

void UkdSettingsWidget::UpdateVolumeLabel(UTextBlock* Label, float Value)
{
    if (!Label) return;
    Label->SetText(FText::FromString(
        FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f))));
}

void UkdSettingsWidget::OnMasterSliderChanged(float Value)
{
    UpdateVolumeLabel(Txt_Master, Value);
    bHasUnsavedChanges = true;
}

void UkdSettingsWidget::OnMusicSliderChanged(float Value)
{
    UpdateVolumeLabel(Txt_Music, Value);
    bHasUnsavedChanges = true;
}

void UkdSettingsWidget::OnSFXSliderChanged(float Value)
{
    UpdateVolumeLabel(Txt_SFX, Value);
    bHasUnsavedChanges = true;
}

void UkdSettingsWidget::OnSensitivityChanged(float Value)
{
    UpdateVolumeLabel(Txt_Sensitivity, Value);
    bHasUnsavedChanges = true;
}

void UkdSettingsWidget::OnQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    bHasUnsavedChanges = true;
}

void UkdSettingsWidget::OnFullscreenChanged(bool bIsChecked)
{
    bHasUnsavedChanges = true;
}

void UkdSettingsWidget::OnVSyncChanged(bool bIsChecked)
{
    bHasUnsavedChanges = true;
}

void UkdSettingsWidget::OnApplyClicked()
{
    FkdGameSettings NewSettings;
    GatherUIIntoSettings(NewSettings);

    if (UkdGameInstance* GI = UkdGameInstance::Get(GetWorld()))
    {
        GI->CommitSettings(NewSettings);
    }
    bHasUnsavedChanges = false;
}

void UkdSettingsWidget::OnBackClicked()
{
    if (bHasUnsavedChanges)
    {
        // In a full game you'd show a "Discard changes?" confirmation dialog here.
        // For now, silently discard and close.
        LoadCurrentSettingsToUI();
    }

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
        {
            HUD->HideSettings();
        }
    }
}

void UkdSettingsWidget::OnResetDefaultsClicked()
{
    UkdGameInstance* GI = UkdGameInstance::Get(GetWorld());
    if (!GI) return;

    GI->CommitSettings(FkdGameSettings{});   // default-constructed = all defaults
    LoadCurrentSettingsToUI();
}
