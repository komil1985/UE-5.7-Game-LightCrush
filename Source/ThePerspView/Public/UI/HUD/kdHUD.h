// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "kdHUD.generated.h"

class UkdMainMenuWidget;
class UkdSettingsWidget;
class UkdPauseMenuWidget;
class UkdLevelCompleteWidget;
class UkdLoadingScreenWidget;
class UkdDeathWidget;
class UkdGameOverWidget;
class UkdStaminaWidget;
class UUserWidget;
/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdHUD : public AHUD
{
	GENERATED_BODY()
	
public:
    // ── Widget Class References (assign in Blueprint subclass) ────────────────
    UPROPERTY(EditDefaultsOnly, Category = "UI | Classes")
    TSubclassOf<UkdMainMenuWidget> MainMenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI | Classes")
    TSubclassOf<UkdSettingsWidget> SettingsWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI | Classes")
    TSubclassOf<UkdPauseMenuWidget> PauseMenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI | Classes")
    TSubclassOf<UkdLevelCompleteWidget> LevelCompleteWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI | Classes")
    TSubclassOf<UkdLoadingScreenWidget> LoadingScreenWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI | Classes")
    TSubclassOf<UkdDeathWidget> DeathWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI | Classes")
    TSubclassOf<UkdGameOverWidget> GameOverWidgetClass;

    // ── Show / Hide API ───────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowMainMenu();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowSettings(bool bFromPause = false);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideSettings();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowPauseMenu();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void HidePauseMenu();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowLevelCompleteScreen(float CompletionTime, int32 Score);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowDeathFeedback(int32 RemainingLives);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowLoadingScreen();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideLoadingScreen();

    /**
     * Show the "YOU DIED" overlay while the screen is black.
     * Called by GameMode after the DeathComponent's fade completes.
     */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowDeathOverlay(int32 RemainingLives);

    /**
     * Hide the death overlay before the respawn fade-in starts.
     * Called by GameMode just before PerformRespawn().
     */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideDeathOverlay();

    /**
     * Show the full-screen Game Over screen.
     * Called by GameMode when RemainingLives == 0.
     */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowGameOver(int32 FinalScore);

    // ── Widget Accessors ──────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "UI")
    UkdPauseMenuWidget* GetPauseMenu() const { return PauseMenuWidget; }

    UFUNCTION(BlueprintPure, Category = "UI")
    UkdSettingsWidget* GetSettingsWidget() const { return SettingsWidget; }

    UFUNCTION(BlueprintPure, Category = "UI")
    UkdGameOverWidget* GetGameOverWidget()  const { return GameOverWidget; }

    // ── Input Mode Helpers ────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetGameInputMode();   // hides cursor, game+UI

    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetUIInputMode();     // shows cursor, UI only

protected:
	virtual void BeginPlay() override;

private:
    // Live widget instances (null until first shown)
    UPROPERTY()
    TObjectPtr<UkdMainMenuWidget> MainMenuWidget;

    UPROPERTY()
    TObjectPtr<UkdSettingsWidget> SettingsWidget;

    UPROPERTY()
    TObjectPtr<UkdPauseMenuWidget> PauseMenuWidget;

    UPROPERTY()
    TObjectPtr<UkdLevelCompleteWidget> LevelCompleteWidget;

    UPROPERTY()
    TObjectPtr<UkdLoadingScreenWidget> LoadingScreenWidget;

    UPROPERTY() 
    TObjectPtr<UkdDeathWidget> DeathWidget;

    UPROPERTY()
    TObjectPtr<UkdGameOverWidget> GameOverWidget;

    // Whether settings were opened from the pause menu (affects "back" behaviour)
    bool bSettingsFromPause = false;

    // Utility: create or retrieve a widget instance
    template<typename TWidget>
    TWidget* GetOrCreate(TObjectPtr<TWidget>& CachedPtr, TSubclassOf<TWidget> WidgetClass, int32 ZOrder = 10);

    UFUNCTION()
    APlayerController* GetOwnerPC() const;
};


// ─────────────────────────────────────────────────────────────────────────────
// AkdHUD — single point of truth for all on-screen widgets.
//
// SETUP (Details panel of your HUD Blueprint subclass):
//   • Assign each WidgetClass property to the matching Blueprint widget.
//
// CALLING CONVENTION:
//   • All Show*/Hide* functions are safe to call from C++ or Blueprint.
//   • Only one "full-screen" widget (menu / level complete) is shown at a time.
//   • The gameplay HUD (stamina bar etc.) is overlaid on top when in-game.
// ─────────────────────────────────────────────────────────────────────────────

template<typename TWidget>
inline TWidget* AkdHUD::GetOrCreate(TObjectPtr<TWidget>& CachedPtr, TSubclassOf<TWidget> WidgetClass, int32 ZOrder)
{
    if (CachedPtr) return CachedPtr;
    if (!WidgetClass) return nullptr;

    APlayerController* PC = GetOwnerPC();
    if (!PC) return nullptr;

    CachedPtr = CreateWidget<TWidget>(PC, WidgetClass);
    if (CachedPtr)
    {
        CachedPtr->AddToViewport(ZOrder);
        CachedPtr->SetVisibility(ESlateVisibility::Collapsed);   // hidden by default
    }
    return CachedPtr;
}
