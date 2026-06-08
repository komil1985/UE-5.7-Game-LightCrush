// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "kdLightHealthWidget.generated.h"



class UProgressBar;
class UTextBlock;
class UOverlay;
class UkdLightHealthComponent;
class UkdColorTheme;
struct FGameplayTag;
UCLASS()
class THEPERSPVIEW_API UkdLightHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
    virtual void NativeDestruct() override;

    // ── Initialisation ────────────────────────────────────────────────────────

    /**
     * Wires up the component delegates and initialises the bar state.
     * Called by AkdMyPlayer::BeginPlay after widget creation.
     */
    //UFUNCTION(BlueprintCallable, Category = "Light Health")
    //void InitializeWithComponent(UkdLightHealthComponent* InComponent);

    void InitializeWithASC(UAbilitySystemComponent* InASC, UkdLightHealthComponent* InComponent);

    void ShowWidget();
    void HideWidget();

    // ── Blueprint hooks ───────────────────────────────────────────────────────

    /** Play your FadeIn / FadeOut UMG animations here. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Light Health")
    void BP_OnCrushModeChanged(bool bIsInActive);

    /** Play a camera-shake, screen-flash etc. when entering critical state. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Light Health")
    void BP_OnCriticalStateChanged(bool bIsInCritical);

    // ── Colour Config ─────────────────────────────────────────────────────────

    /** Bar fill colour while the player is standing in shadow (healing). */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health | Colour")
    FLinearColor ShadowHealColor = FLinearColor(0.0f, 0.85f, 0.85f, 1.0f);   // teal

    /** Bar fill colour while the player is exposed to light (taking damage). */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health | Colour")
    FLinearColor LightDamageColor = FLinearColor(0.9f, 0.15f, 0.05f, 1.0f);  // red-orange

    /** Speed at which the bar colour lerps between shadow and light states. */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health | Colour",
        meta = (ClampMin = "0.5", ClampMax = "20.0"))
    float ColourLerpSpeed = 6.0f;

    /** Speed at which the bar fill value lerps to the new health value. */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health | Colour",
        meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float BarLerpSpeed = 10.0f;

    // ── Critical Warning ──────────────────────────────────────────────────────

    /** Seconds between each danger text pulse (blink on/off). */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health | Warning",
        meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float DangerBlinkInterval = 0.35f;

    /** Below this fraction → critical warning text appears. */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health | Warning",
        meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float CriticalHealthThreshold = 0.25f;

protected:
    // ── UMG Bindings ──────────────────────────────────────────────────────────

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> Bar_LightHealth;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Txt_LightDanger;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UOverlay> Overlay_Root;

private:
    // ── Component reference ───────────────────────────────────────────────────

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> CachedASC;

    UPROPERTY()
    TObjectPtr<UkdLightHealthComponent> CachedComponent;

    // ── Runtime state ─────────────────────────────────────────────────────────

    float DisplayHealth = 1.f;      // smoothly lerped display value [0, 1]
    float TargetHealth = 1.f;      // raw value from component

    FLinearColor CurrentBarColor;   // lerped each frame
    FLinearColor TargetBarColor;

    bool bInCrushMode = false;
    bool bInShadow = false;
    bool bIsCritical = false;
    float DangerTimer = 0.f;      // accumulates for blink cycle

    // ── Delegate handles (for clean unbind on Destruct) ───────────────────────

    FDelegateHandle HealthChangedHandle;
    FDelegateHandle CriticalChangedHandle;
    FDelegateHandle ShadowStateChangedHandle;

    // ── GAS attribute callbacks (same as StaminaWidget) ───────────────────────
    void OnLightHealthChanged(const FOnAttributeChangeData& Data);
    void OnMaxLightHealthChanged(const FOnAttributeChangeData& Data);

    // ── Tag callbacks (registered on ASC) ────────────────────────────────────

    //void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);

    // ── Component delegate callbacks ──────────────────────────────────────────

    //void OnHealthChanged(float Current, float Max);
    void OnCriticalChanged(bool bNewCritical);
    void OnShadowStateChanged(bool bNewInShadow);

    // ── Tick helpers ──────────────────────────────────────────────────────────

    void TickBarLerp(float DeltaTime);
    void TickColourLerp(float DeltaTime);
    void TickDangerBlink(float DeltaTime);

    void UpdateBar(float CurrentHealth, float MaxHealth);

    /** Resolves target bar colour from theme + current state. */
    FLinearColor ResolveTargetBarColor() const;
};
