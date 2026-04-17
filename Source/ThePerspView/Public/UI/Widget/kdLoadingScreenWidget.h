// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdLoadingScreenWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API UkdLoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** HUD calls this right before making the widget visible. */
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void StartAnimation();

    // Minimum seconds the loading screen stays visible so it doesn't flash
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
    float MinDisplayTime = 1.5f;

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> Bar_Progress;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock>   Txt_LoadingHint;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage>       Img_Background;

private:
    float ElapsedTime = 0.f;
    float FakeProgress = 0.f;    // smoothly fills bar while level streams
    int32 HintIndex = 0;

    // Gameplay tips displayed in rotation
    static const TArray<FString> LoadingHints;

    void AdvanceHint();
    void UpdateProgressBar(float DeltaTime);

    FTimerHandle HintCycleHandle;
};


// ─────────────────────────────────────────────────────────────────────────────
// UkdLoadingScreenWidget
//
// Blueprint UMG bindings required:
//   Bar_Progress        (UProgressBar) – optional; can be driven to 100% on timer
//   Txt_LoadingHint     (UTextBlock)   – cycling gameplay tips
//   Img_Background      (UImage)       – full-screen art or gradient
//
// USAGE:
//   HUD->ShowLoadingScreen() before OpenLevel().
//   UE's async level-load will replace this screen once streaming completes.
//   For maps that load fast, the screen shows for a minimum of MinDisplayTime.
// ─────────────────────────────────────────────────────────────────────────────