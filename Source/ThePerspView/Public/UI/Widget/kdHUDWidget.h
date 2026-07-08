// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/ColorLibrary/kdHUDColorRamp.h"
#include "kdHUDWidget.generated.h"

class UProgressBar;

/**
 * UkdHUDWidget
 *
 * Screen-space vitals HUD (Health + Stamina). Pure view: values are pushed in by
 * UkdPlayerHUDComponent; the widget owns only presentation (fill % + themed color).
 *
 * Color is driven by two FkdBarColorRamps (Heliograph defaults set in the constructor,
 * overridable in the WBP or injected at runtime via ApplyBarThemes). Percent -> color is
 * evaluated on every value change; a low-fill "brownout" throb and the stamina exhaust
 * flash are driven in NativeTick (which early-outs when nothing is animating).
 */
UCLASS()
class THEPERSPVIEW_API UkdHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UkdHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** Push fill fractions (clamped to [0,1]). */
	void SetHealthPercent(float InPercent01);
	void SetStaminaPercent(float InPercent01);

	/** Stamina-lockout flash (drive from the State.Exhausted tag). */
	void SetExhausted(bool bInExhausted);

	/** Optional: inject themed ramps (e.g. from UkdThemeSubsystem / DA_Heliograph). */
	void ApplyBarThemes(const FkdBarColorRamp& InHealthRamp, const FkdBarColorRamp& InStaminaRamp);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void RefreshHealthVisual();
	void RefreshStaminaVisual();

	/** WBP element names must match exactly (BindWidget contract). */
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> Bar_Health = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> Bar_Stamina = nullptr;

	// ── Theme (Heliograph defaults in ctor; override in WBP or via ApplyBarThemes) ──
	UPROPERTY(EditAnywhere, Category = "kd|HUD|Theme") FkdBarColorRamp HealthRamp;
	UPROPERTY(EditAnywhere, Category = "kd|HUD|Theme") FkdBarColorRamp StaminaRamp;

	/** Below this health fraction the bar throbs (failing-light heartbeat). */
	UPROPERTY(EditAnywhere, Category = "kd|HUD|Theme", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowPulseThreshold = 0.28f;

	/** Pulse angular speed (rad/s). ~6 ≈ one throb/sec. */
	UPROPERTY(EditAnywhere, Category = "kd|HUD|Theme") float PulseSpeed = 6.0f;

	/** Brightness swing of the throb (0..1). */
	UPROPERTY(EditAnywhere, Category = "kd|HUD|Theme", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PulseDepth = 0.22f;

	/** Color the stamina bar flashes toward while exhausted. */
	UPROPERTY(EditAnywhere, Category = "kd|HUD|Theme") FLinearColor ExhaustFlashColor;

private:
	float Health01 = 1.f;
	float Stamina01 = 1.f;
	bool  bExhausted = false;
	float PulsePhase = 0.f;

	// Un-pulsed base colors, cached so NativeTick can modulate without re-evaluating.
	FLinearColor HealthBaseColor = FLinearColor::White;
	FLinearColor StaminaBaseColor = FLinearColor::White;
};
