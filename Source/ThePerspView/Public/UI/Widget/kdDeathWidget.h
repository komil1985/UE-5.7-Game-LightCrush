// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdDeathWidget.generated.h"

class UTextBlock;
class UImage;
class UOverlay;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdDeathWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

    /**
     * Call before AddToViewport. Populates live data (lives count) and
     * starts the animated hint text.
     */
    UFUNCTION(BlueprintCallable, Category = "Death")
    void SetupDeath(int32 RemainingLives);

    /** Called by HUD when it's time to hide — triggers fade-out animation. */
    UFUNCTION(BlueprintCallable, Category = "Death")
    void BeginHide();

    // ── Blueprint animation hooks ─────────────────────────────────────────────

    UFUNCTION(BlueprintImplementableEvent, Category = "Death")
    void BP_OnShow();   // play FadeIn animation

    UFUNCTION(BlueprintImplementableEvent, Category = "Death")
    void BP_OnHide();   // play FadeOut animation

    // ── Config ────────────────────────────────────────────────────────────────

    /** Seconds between each dot added to "Respawning..." text. */
    UPROPERTY(EditDefaultsOnly, Category = "Death")
    float HintDotInterval = 0.4f;

protected:
    UPROPERTY(meta = (BindWidget)) 
    TObjectPtr<UTextBlock>Txt_YouDied;

    UPROPERTY(meta = (BindWidget)) 
    TObjectPtr<UTextBlock>Txt_LivesRemaining;

    UPROPERTY(meta = (BindWidget)) 
    TObjectPtr<UTextBlock>Txt_RespawnHint;

    UPROPERTY(meta = (BindWidget)) 
    TObjectPtr<UImage>Img_Vignette;

    UPROPERTY(meta = (BindWidget)) 
    TObjectPtr<UOverlay>Overlay_Content;

private:
    float  DotTimer = 0.f;
    int32  DotCount = 0;

    void UpdateRespawnDots(float DeltaTime);
};


// ─────────────────────────────────────────────────────────────────────────────
// UkdDeathWidget — transient overlay shown during the respawn window.
//
// This widget appears AFTER the camera fade goes black and disappears BEFORE
// the fade-back-in, so the player never sees it layered over gameplay.
// It gives the player two pieces of feedback:
//   1. "YOU DIED" — confirmation of what happened
//   2. Remaining lives count — so they know their resource state
//
// Blueprint UMG bindings required:
//   Txt_YouDied       (TextBlock)  – "YOU DIED" header
//   Txt_LivesRemaining(TextBlock)  – "2 Lives Remaining" or "Last Life!"
//   Txt_RespawnHint   (TextBlock)  – "Respawning..." cycling dots
//   Img_Vignette      (Image)      – full-screen dark overlay (set to screen size)
//   Overlay_Content   (Overlay)    – wraps text elements for fade animation
//
// ANIMATION:
//   Add UMG animations:
//     "FadeIn"   – Overlay_Content opacity 0→1 over 0.3s
//     "FadeOut"  – Overlay_Content opacity 1→0 over 0.3s
//   Wire them via BP_OnShow / BP_OnHide.
// ─────────────────────────────────────────────────────────────────────────────