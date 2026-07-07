// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"        // FOnAttributeChangeData
#include "kdPlayerHUDComponent.generated.h"

class UkdHUDWidget;
class UAbilitySystemComponent;
class APlayerController;
class APawn;

/**
 * UkdPlayerHUDComponent
 *
 * Single-writer owner of the on-screen (viewport) player HUD. Lives on AkdPlayerController.
 *
 *   - Creates the root HUD widget once and AddToViewport (screen-space, fixed layout).
 *   - Subscribes to the possessed pawn's ASC Health / Stamina change delegates.
 *   - Pushes normalized values into the pure-view widget.
 *   - Tears everything down cleanly on unpossess / end play / level transition.
 *
 * Replaces the previous world-space UWidgetComponent bars parented above the player's
 * head. Because the widget lives in the viewport (not a WidgetComponent), show/hide is
 * a cheap SetVisibility call that never destroys/recreates the widget.
 */
UCLASS(ClassGroup = (kd), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdPlayerHUDComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UkdPlayerHUDComponent();

    /**
     * Bind to a pawn's ASC and seed the bars. Call from AkdPlayerController::OnPossess.
     * Idempotent: safe to call repeatedly; unbinds any previous ASC first.
     */
    void InitializeForPawn(APawn* InPawn);

    /** Unbind from the current ASC. Call from AkdPlayerController::OnUnPossess. */
    void ReleasePawn();

    /** Cheap screen HUD show/hide (does NOT destroy the widget). */
    void SetHUDVisible(bool bVisible);

    /** WBP class to instantiate. Set in the controller constructor (see wiring below). */
    UPROPERTY(EditAnywhere, Category = "kd|HUD")
    TSubclassOf<UkdHUDWidget> HUDWidgetClass = nullptr;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void EnsureWidget();   // create + AddToViewport (once)
    void DestroyWidget();  // remove + clear

    void HandleHealthChanged(const FOnAttributeChangeData& Data);
    void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
    void HandleStaminaChanged(const FOnAttributeChangeData& Data);
    void HandleMaxStaminaChanged(const FOnAttributeChangeData& Data);

    void RefreshHealthBar() const;
    void RefreshStaminaBar() const;

    APlayerController* GetOwningPlayerController() const;

private:
    /** Draw order for AddToViewport. HUD sits under menus/dialogs. */
    UPROPERTY(EditAnywhere, Category = "kd|HUD")
    int32 ViewportZOrder = 10;

    UPROPERTY(Transient)
    TObjectPtr<UkdHUDWidget> HUDWidget = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UAbilitySystemComponent> BoundASC = nullptr;

    FDelegateHandle HealthChangedHandle;
    FDelegateHandle MaxHealthChangedHandle;
    FDelegateHandle StaminaChangedHandle;
    FDelegateHandle MaxStaminaChangedHandle;

    float CachedHealth = 0.f;
    float CachedMaxHealth = 1.f;
    float CachedStamina = 0.f;
    float CachedMaxStamina = 1.f;
};
