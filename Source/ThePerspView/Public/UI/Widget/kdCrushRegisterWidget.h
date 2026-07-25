// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Crush/kdCrushDirection.h"
#include "Components/kdCrushAlignmentComponent.h"
#include "kdCrushRegisterWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * UkdCrushRegisterWidget  —  the split-prism rangefinder
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Screen-space readout for UkdCrushAlignmentComponent. Borrows the one focus
 * aid every 19th/20th-century camera user already understands: a split-image
 * rangefinder. Two half-marks sit apart when the view is off-axis, slide toward
 * each other as the camera approaches a crush-legal cardinal, and butt together
 * with a click at lock. No numbers, no compass, no tutorial text — the player
 * reads "not yet / nearly / now" at a glance from a single converging shape.
 *
 * Design rules this widget obeys:
 *
 *   1. INVISIBLE AT REST. Opacity tracks Register01, which is zero until the
 *      player is already inside the capture arc — i.e. until they have already
 *      started aiming. It never adds clutter to normal 3D traversal.
 *
 *   2. PURE VIEW, PURE PULL. Values are read from the component in NativeTick.
 *      There is no presenter push and no initialisation-order dependency; the
 *      same robustness argument as UkdTransitionFlashWidget. Only the two
 *      genuinely event-shaped cues (lock punch, denial flash) use delegates.
 *
 *   3. WRITES NOTHING. It reads the component and paints. It cannot affect
 *      gameplay, so it can never be the cause of a crush bug.
 *
 * ── WBP CONTRACT ────────────────────────────────────────────────────────────
 * Reparent WBP_CrushRegister to this class. Every binding is OPTIONAL, so a
 * partially-built WBP will not crash — it will simply animate less. Names must
 * match exactly:
 *
 *   Img_MarkLeft    UImage      left half-mark   (anchor centre, translated -X)
 *   Img_MarkRight   UImage      right half-mark  (anchor centre, translated +X)
 *   Img_Frame       UImage      plate frame / reticle ring (punches on lock)
 *   Txt_Axis        UTextBlock  cardinal label, e.g. "N"
 *
 * Anchor the whole widget centre-screen, slightly below the crosshair line.
 */
UCLASS()
class THEPERSPVIEW_API UkdCrushRegisterWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
    // ── Bound elements (all optional — see WBP contract above) ──────────────
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     Img_MarkLeft = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     Img_MarkRight = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     Img_Frame = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Axis = nullptr;

    // ── Geometry ────────────────────────────────────────────────────────────

    /** Half-separation of the marks at Register01 = 0, in slate units.
     *  They meet exactly at Register01 = 1. */
    UPROPERTY(EditAnywhere, Category = "kd|Register|Layout", meta = (ClampMin = "0.0"))
    float MaxMarkSeparation = 90.f;

    /** Interp speed for opacity and separation. High enough to feel responsive,
     *  low enough to smooth per-frame mouse jitter into readable motion. */
    UPROPERTY(EditAnywhere, Category = "kd|Register|Layout", meta = (ClampMin = "1.0"))
    float VisualInterpSpeed = 14.f;

    /** Below this smoothed register value the widget is fully transparent. */
    UPROPERTY(EditAnywhere, Category = "kd|Register|Layout",
        meta = (ClampMin = "0.0", ClampMax = "0.9"))
    float FadeInFloor = 0.02f;

    // ── Colour (Heliograph palette defaults set in NativeConstruct) ─────────

    /** Colour at the far edge of the capture arc. */
    UPROPERTY(EditAnywhere, Category = "kd|Register|Theme") FLinearColor SeekingColor;

    /** Colour at lock — the exposure has taken. */
    UPROPERTY(EditAnywhere, Category = "kd|Register|Theme") FLinearColor LockedColor;

    /** Colour flashed when a press is genuinely refused. */
    UPROPERTY(EditAnywhere, Category = "kd|Register|Theme") FLinearColor DeniedColor;

    // ── One-shot cues ───────────────────────────────────────────────────────

    /** Extra scale added to Img_Frame at the instant of lock (0.25 = 125%). */
    UPROPERTY(EditAnywhere, Category = "kd|Register|Cues",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LockPunchScale = 0.22f;

    UPROPERTY(EditAnywhere, Category = "kd|Register|Cues", meta = (ClampMin = "0.02"))
    float LockPunchDuration = 0.18f;

    /** How long the denial flash holds before fading. */
    UPROPERTY(EditAnywhere, Category = "kd|Register|Cues", meta = (ClampMin = "0.05"))
    float DeniedFlashDuration = 0.35f;

    // ── Axis labels (exposed for localisation) ──────────────────────────────
    UPROPERTY(EditAnywhere, Category = "kd|Register|Labels") FText Label_PosX;
    UPROPERTY(EditAnywhere, Category = "kd|Register|Labels") FText Label_NegX;
    UPROPERTY(EditAnywhere, Category = "kd|Register|Labels") FText Label_PosY;
    UPROPERTY(EditAnywhere, Category = "kd|Register|Labels") FText Label_NegY;

    // ── Delegate handlers (the only two genuinely event-shaped cues) ────────
    UFUNCTION()
    void HandleRegisterStateChanged(EkdRegisterState NewState, EkdCrushDirection PredictedDirection);

    UFUNCTION()
    void HandleIntentResolved(bool bSucceeded);

private:
    /** Lazily find (and re-find, after respawn) the pawn's alignment component. */
    UkdCrushAlignmentComponent* ResolveAlignment();

    /** Bind/unbind the dynamic delegates when the resolved component changes. */
    void BindTo(UkdCrushAlignmentComponent* NewComp);

    FText AxisLabelFor(EkdCrushDirection Direction) const;

    TWeakObjectPtr<UkdCrushAlignmentComponent> Alignment;

    float SmoothedRegister01 = 0.f;
    float LockPunchTimer = 0.f;
    float DeniedFlashTimer = 0.f;
};
