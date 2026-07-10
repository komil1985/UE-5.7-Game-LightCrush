// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdTutorialPromptWidget.generated.h"

class UTextBlock;
class UBorder;

/**
 * UkdTutorialPromptWidget — the Heliograph Plate.
 *
 * PURE VIEW. UkdTutorialSubsystem is the sole writer; this class owns only
 * presentation. Nothing else may call ShowStep/PlayDismiss.
 *
 * REVEAL SEQUENCE (the "develop"):
 *   Developing : render opacity 0→1 over DevelopSeconds, plate settles from
 *                102% to 100% scale — an emulsion drying down onto the plate.
 *   Typing     : body text exposes character-by-character with a block cursor.
 *   Held       : full copy, cursor removed.
 *   Dismissing : opacity 1→0 over DismissSeconds.
 *
 * WBP CONTRACT (names must match exactly):
 *   Txt_Headline (TextBlock, required)
 *   Txt_Body     (TextBlock, required)
 *   Txt_Key      (TextBlock, optional)   — resolved key glyph, e.g. "Space"
 *   Border_Key   (Border,    optional)   — the key cap; hidden when no glyph
 */
UCLASS()
class THEPERSPVIEW_API UkdTutorialPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UkdTutorialPromptWidget(const FObjectInitializer& ObjectInitializer);

    void ShowStep(const FText& InHeadline, const FText& InBody, const FText& InKeyGlyph);
    void PlayDismiss();
    void HideImmediate();

    /** Subsystem reads this to know how long to stay in its Dismissing phase. */
    FORCEINLINE float GetDismissSeconds() const { return DismissSeconds; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> Txt_Headline = nullptr;
    UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> Txt_Body = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Key = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder>    Border_Key = nullptr;

    UPROPERTY(EditAnywhere, Category = "kd|Tutorial", meta = (ClampMin = "1.0"))
    float CharsPerSecond = 45.0f;

    UPROPERTY(EditAnywhere, Category = "kd|Tutorial", meta = (ClampMin = "0.0"))
    float DevelopSeconds = 0.45f;

    UPROPERTY(EditAnywhere, Category = "kd|Tutorial", meta = (ClampMin = "0.01"))
    float DismissSeconds = 0.35f;

    /** Body/headline ink. Heliograph defaults baked in the constructor. */
    UPROPERTY(EditAnywhere, Category = "kd|Tutorial|Theme") FLinearColor HeadlineColor;
    UPROPERTY(EditAnywhere, Category = "kd|Tutorial|Theme") FLinearColor BodyColor;
    UPROPERTY(EditAnywhere, Category = "kd|Tutorial|Theme") FLinearColor KeyCapColor;

private:
    enum class EPhase : uint8 { Hidden, Developing, Typing, Held, Dismissing };

    void ApplyBodyReveal(int32 VisibleChars, bool bShowCursor);

    EPhase  Phase = EPhase::Hidden;
    float   PhaseTime = 0.f;
    FString FullBody;
    int32   LastRevealed = -1;
};