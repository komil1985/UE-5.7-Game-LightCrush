// Copyright ASKD Games


#include "UI/ColorLibrary/kdHUDColorLibrary.h"
#include "Data/kdColorTheme.h"




bool UkdHUDColorLibrary::ValidateTheme(const UkdColorTheme* Theme, const TCHAR* CallerName)
{
    if (!Theme)
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Warning,
            TEXT("HUDColorLibrary::%s — ColorTheme is null. Returning white."),
            CallerName);
#endif
        return false;
    }
    return true;
}

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

float UkdHUDColorLibrary::GetExhaustedPulseAlpha(float TimeSeconds, float PulseRate)
{
    // Sine wave remapped from [-1, 1] to [0, 1]
    // Peaks once every (1 / PulseRate) seconds — default 3 Hz = fast warning pulse
    return 0.5f + 0.5f * FMath::Sin(TimeSeconds * PulseRate * TWO_PI);
}

FLinearColor UkdHUDColorLibrary::GetTransitionFlashColor(const UkdColorTheme* Theme, float BlendAlpha)
{
    if (!ValidateTheme(Theme, TEXT("GetTransitionFlashColor")))
    {
        return FLinearColor::Transparent;
    }

    // sin(Alpha * PI) peaks at Alpha = 0.5 (mid-transition) and is zero at
    // both ends — smooth in/out flash that never blocks the screen at rest.
    const float Opacity = FMath::Sin(FMath::Clamp(BlendAlpha, 0.f, 1.f) * PI);

    return FLinearColor(
        Theme->CrushTeal.R,
        Theme->CrushTeal.G,
        Theme->CrushTeal.B,
        Opacity * 0.35f);   // 35 % max opacity — visible but not blinding
}


FLinearColor UkdHUDColorLibrary::GetPortalGlowColor(const UkdColorTheme* Theme, float StaminaFraction)
{
    if (!ValidateTheme(Theme, TEXT("GetPortalGlowColor")))
    {
        return FLinearColor::White;
    }

    // Full stamina → Spectre (glowing, inviting).
    // Empty stamina → Slate  (desaturated, communicates "not usable right now").
    return FLinearColor::LerpUsingHSV(
        Theme->Slate,
        Theme->Spectre,
        FMath::Clamp(StaminaFraction, 0.f, 1.f));
}