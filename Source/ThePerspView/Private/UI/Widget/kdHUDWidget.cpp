// Copyright ASKD Games

#include "UI/Widget/kdHUDWidget.h"
#include "Components/ProgressBar.h"


UkdHUDWidget::UkdHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// ── Health: life-danger ramp ────────────────────────────────────────────────
	// Cool ion-blue at full → warms to gold → burns to exhaust red as death nears.
	HealthRamp.Stops = {
		{ 0.00f, kdHeliograph::ExhaustRed },
		{ 0.28f, kdHeliograph::EmberTrace },
		{ 0.55f, kdHeliograph::GoldLeaf   },
		{ 1.00f, kdHeliograph::PaleIon    },
	};

	// ── Stamina: resource ramp (NOT death) ──────────────────────────────────────
	// Bright solar gold at full → dims to steel as the shadow energy empties.
	StaminaRamp.Stops = {
		{ 0.00f, kdHeliograph::Steelgrey },
		{ 0.15f, kdHeliograph::Steelgrey },
		{ 0.50f, kdHeliograph::Sunmark   },
		{ 1.00f, kdHeliograph::GoldLeaf  },
	};

	ExhaustFlashColor = kdHeliograph::EmberTrace;
}

void UkdHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Sub-widgets are bound now; apply whatever the presenter pushed pre-construct.
	RefreshHealthVisual();
	RefreshStaminaVisual();
}

void UkdHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const bool bHealthLow = (Health01 <= LowPulseThreshold);

	// Nothing animating → leave the base colors untouched (near-free early-out).
	if (!bHealthLow && !bExhausted)
	{
		return;
	}

	PulsePhase += InDeltaTime * PulseSpeed;
	const float Wave = 0.5f * (FMath::Sin(PulsePhase) + 1.f); // 0..1

	// Low health: throb brightness and bloom slightly toward SolarWhite at the crest,
	// so the bar reads like a failing exposure rather than a flat red block.
	if (bHealthLow && IsValid(Bar_Health))
	{
		const float Brightness = (1.f - PulseDepth) + PulseDepth * Wave;
		FLinearColor C = HealthBaseColor * Brightness;
		C = FMath::Lerp(C, kdHeliograph::SolarWhite, 0.15f * Wave);
		C.A = 1.f;
		Bar_Health->SetFillColorAndOpacity(C);
	}

	// Exhausted: flash the stamina bar toward ember (distinct from its steady dim).
	if (bExhausted && IsValid(Bar_Stamina))
	{
		Bar_Stamina->SetFillColorAndOpacity(FMath::Lerp(StaminaBaseColor, ExhaustFlashColor, Wave));
	}
}

void UkdHUDWidget::SetHealthPercent(float InPercent01)
{
	Health01 = FMath::Clamp(InPercent01, 0.f, 1.f);
	RefreshHealthVisual();
}

void UkdHUDWidget::SetStaminaPercent(float InPercent01)
{
	Stamina01 = FMath::Clamp(InPercent01, 0.f, 1.f);
	RefreshStaminaVisual();
}

void UkdHUDWidget::SetExhausted(bool bInExhausted)
{
	bExhausted = bInExhausted;
	if (!bExhausted)
	{
		// Restore the steady ramp color the instant the lockout clears.
		RefreshStaminaVisual();
	}
}

void UkdHUDWidget::ApplyBarThemes(const FkdBarColorRamp& InHealthRamp, const FkdBarColorRamp& InStaminaRamp)
{
	if (InHealthRamp.Stops.Num() > 0) { HealthRamp = InHealthRamp; }
	if (InStaminaRamp.Stops.Num() > 0) { StaminaRamp = InStaminaRamp; }

	RefreshHealthVisual();
	RefreshStaminaVisual();
}

void UkdHUDWidget::RefreshHealthVisual()
{
	if (!IsValid(Bar_Health)) { return; }

	HealthBaseColor = HealthRamp.Evaluate(Health01);
	Bar_Health->SetPercent(Health01);
	Bar_Health->SetFillColorAndOpacity(HealthBaseColor); // pulse (if low) overrides in tick
}

void UkdHUDWidget::RefreshStaminaVisual()
{
	if (!IsValid(Bar_Stamina)) { return; }

	StaminaBaseColor = StaminaRamp.Evaluate(Stamina01);
	Bar_Stamina->SetPercent(Stamina01);
	Bar_Stamina->SetFillColorAndOpacity(StaminaBaseColor); // exhaust flash overrides in tick
}
