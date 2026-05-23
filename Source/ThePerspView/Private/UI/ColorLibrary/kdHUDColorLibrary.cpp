// Copyright ASKD Games

#include "UI/ColorLibrary/kdHUDColorLibrary.h"
#include "Data/kdColorTheme.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal validator
// ─────────────────────────────────────────────────────────────────────────────

bool UkdHUDColorLibrary::ValidateTheme(const UkdColorTheme* Theme, const TCHAR* CallerName)
{
    if (!Theme)
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Warning,
            TEXT("HUDColorLibrary::%s — ColorTheme is null.  Returning white."),
            CallerName);
#endif
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Bar fills + world colours — forward to the data asset's helpers
// ─────────────────────────────────────────────────────────────────────────────

FLinearColor UkdHUDColorLibrary::GetStaminaBarColor(const UkdColorTheme* Theme, float FillFraction)
{
    if (!ValidateTheme(Theme, TEXT("GetStaminaBarColor")))
    {
        return FLinearColor::White;
    }
    return Theme->GetStaminaBarColor(FillFraction);
}

FLinearColor UkdHUDColorLibrary::GetHealthBarColor(const UkdColorTheme* Theme, float FillFraction)
{
    if (!ValidateTheme(Theme, TEXT("GetHealthBarColor")))
    {
        return FLinearColor::White;
    }
    return Theme->GetHealthBarColor(FillFraction);
}

FLinearColor UkdHUDColorLibrary::GetWorldTextColor(const UkdColorTheme* Theme, bool bInCrushMode)
{
    if (!ValidateTheme(Theme, TEXT("GetWorldTextColor")))
    {
        return FLinearColor::White;
    }
    return Theme->GetWorldTextColor(bInCrushMode);
}

FLinearColor UkdHUDColorLibrary::GetHUDPanelColor(const UkdColorTheme* Theme, bool bInCrushMode)
{
    if (!ValidateTheme(Theme, TEXT("GetHUDPanelColor")))
    {
        return FLinearColor::White;
    }
    return Theme->GetHUDPanelColor(bInCrushMode);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pulse / flash helpers
// ─────────────────────────────────────────────────────────────────────────────

float UkdHUDColorLibrary::GetExhaustedPulseAlpha(float TimeSeconds, float PulseRate)
{
    // Sine remapped from [-1, 1] to [0, 1].  Default 3 Hz = fast danger pulse.
    return 0.5f + 0.5f * FMath::Sin(TimeSeconds * PulseRate * TWO_PI);
}

FLinearColor UkdHUDColorLibrary::GetTransitionFlashColor(const UkdColorTheme* Theme, float BlendAlpha)
{
    if (!ValidateTheme(Theme, TEXT("GetTransitionFlashColor")))
    {
        return FLinearColor::Transparent;
    }

    // sin(α · π) peaks at α = 0.5 and is zero at both ends — a smooth
    // exposure flash that never blocks the screen at rest.
    const float Opacity = FMath::Sin(FMath::Clamp(BlendAlpha, 0.f, 1.f) * PI);

    // Heliograph uses Lumen (warm white-gold) for the flash — reads as the
    // moment of exposure, not a coloured screen wipe.
    return FLinearColor(
        Theme->Lumen.R,
        Theme->Lumen.G,
        Theme->Lumen.B,
        Opacity * 0.25f);   // 25 % max opacity — visible but never blinding
}

FLinearColor UkdHUDColorLibrary::GetPortalGlowColor(const UkdColorTheme* Theme, float StaminaFraction)
{
    if (!ValidateTheme(Theme, TEXT("GetPortalGlowColor")))
    {
        return FLinearColor::White;
    }

    return FLinearColor::LerpUsingHSV(
        Theme->Steelgrey,
        Theme->PaleIon,
        FMath::Clamp(StaminaFraction, 0.f, 1.f));
}

FLinearColor UkdHUDColorLibrary::GetOutlineGlowColor(const UkdColorTheme* Theme, bool bIsHazard)
{
    if (!ValidateTheme(Theme, TEXT("GetOutlineGlowColor")))
    {
        return FLinearColor::White;
    }
    return Theme->GetOutlineGlowColor(bIsHazard);
}
