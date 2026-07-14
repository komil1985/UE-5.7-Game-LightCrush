// Copyright ASKD Games

#include "UI/Widget/kdLightHealthWidget.h"
#include "Components/kdLightHealthComponent.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Data/kdColorTheme.h"
#include "UI/ColorLibrary/kdHUDColorLibrary.h"
#include "UI/ColorLibrary/kdThemeAccess.h"
#include "GameplayTags/kdGameplayTags.h"


// ─────────────────────────────────────────────────────────────────────────────
// NativeConstruct / Destruct
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthWidget::NativeConstruct()
{
    Super::NativeConstruct();

    CurrentBarColor = LightDamageColor;
    TargetBarColor = LightDamageColor;

    if (Bar_LightHealth)
    {
        Bar_LightHealth->SetPercent(1.f);
        Bar_LightHealth->SetFillColorAndOpacity(CurrentBarColor);
    }

    if (Txt_LightDanger)
    {
        Txt_LightDanger->SetVisibility(ESlateVisibility::Hidden);
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("LightHealthWidget: NativeConstruct fired. Bar_LightHealth valid=%d"),
        Bar_LightHealth != nullptr ? 1 : 0);
#endif
}

void UkdLightHealthWidget::NativeDestruct()
{
    // Clean up non-GAS delegate handles
    if (CachedComponent)
    {
        CachedComponent->OnShadowStateChanged.Remove(ShadowStateChangedHandle);
        CachedComponent->OnLightCriticalChanged.Remove(CriticalChangedHandle);
    }

    // GAS delegates are automatically unbound when the UObject is destroyed
    Super::NativeDestruct();
}

// ─────────────────────────────────────────────────────────────────────────────
// InitializeWithASC
// Mirrors UkdStaminaWidget::InitializeWithAbilitySystemComponent exactly —
// the proven working pattern. ASC is passed in directly from the player.
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthWidget::InitializeWithASC(UAbilitySystemComponent* InASC, UkdLightHealthComponent* InComponent)
{
    if (!InASC)
    {
        UE_LOG(LogTemp, Error, TEXT("LightHealthWidget::InitializeWithASC — InASC is NULL! Widget will not update."));
        return;
    }
    if (!InComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("LightHealthWidget::InitializeWithASC — InComponent is NULL!"));
        return;
    }

    CachedASC = InASC;
    CachedComponent = InComponent;

    // ── Bind GAS attribute delegates (same API as StaminaWidget) ─────────────
    // These are fired by ASC->SetNumericAttributeBase, which is used in
    // LightHealthComponent::SetCurrentHealth (fixed version).
    InASC->GetGameplayAttributeValueChangeDelegate(
        UkdAttributeSet::GetLightHealthAttribute())
        .AddUObject(this, &UkdLightHealthWidget::OnLightHealthChanged);

    InASC->GetGameplayAttributeValueChangeDelegate(
        UkdAttributeSet::GetMaxLightHealthAttribute())
        .AddUObject(this, &UkdLightHealthWidget::OnMaxLightHealthChanged);

    // ── Bind component delegates for shadow / critical state ─────────────────
    ShadowStateChangedHandle = InComponent->OnShadowStateChanged.AddUObject(
        this, &UkdLightHealthWidget::OnShadowStateChanged);

    CriticalChangedHandle = InComponent->OnLightCriticalChanged.AddUObject(
        this, &UkdLightHealthWidget::OnCriticalChanged);

    // ── Set initial bar values ────────────────────────────────────────────────
    const float CurrentHealth = InASC->GetNumericAttribute(UkdAttributeSet::GetLightHealthAttribute());
    const float MaxHealth = InASC->GetNumericAttribute(UkdAttributeSet::GetMaxLightHealthAttribute());

    UpdateBar(CurrentHealth, MaxHealth);

    //CurrentBarColor = LightDamageColor;
    //TargetBarColor = LightDamageColor;

    bInShadow = InComponent->IsInShadow();
    TargetBarColor = ResolveTargetBarColor();
    CurrentBarColor = TargetBarColor;
    DangerTimer = 0.f;

    if (Bar_LightHealth)
    {
        Bar_LightHealth->SetFillColorAndOpacity(CurrentBarColor);
    }

    if (Txt_LightDanger)
    {
        Txt_LightDanger->SetVisibility(
            bInShadow ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log,
        TEXT("LightHealthWidget: InitializeWithASC complete — Health=%.0f / %.0f  Bar valid=%d"),
        CurrentHealth, MaxHealth, Bar_LightHealth != nullptr ? 1 : 0);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Visibility (called by AkdMyPlayer::OnCrushModeTagChanged)
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthWidget::ShowWidget()
{
    SetVisibility(ESlateVisibility::HitTestInvisible);
    BP_OnCrushModeChanged(true);

    // Re-sync the bar value immediately so it's current on show
    if (CachedASC)
    {
        const float Current = CachedASC->GetNumericAttribute(UkdAttributeSet::GetLightHealthAttribute());
        const float Max = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxLightHealthAttribute());
        UpdateBar(Current, Max);

        // ── Re-seed the light-exposure warning from ground truth ──────────────
        // The component broadcasts OnShadowStateChanged only on an *edge*.
        // Entering Crush already-under-light produces no edge, so without this
        // the sign can stay stuck hidden from a prior shadow episode. Read the
        // live tag directly so it's correct regardless of multicast order.
        bInShadow = CachedASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_InShadow);
        TargetBarColor = ResolveTargetBarColor();
        DangerTimer = 0.f; // start the blink in its visible phase

        if (Txt_LightDanger)
        {
            Txt_LightDanger->SetVisibility(
                bInShadow ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
        }
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("LightHealthWidget: ShowWidget — health=%.0f%% inShadow=%d"),
        TargetHealth * 100.f, bInShadow);
#endif
}

void UkdLightHealthWidget::HideWidget()
{
    SetVisibility(ESlateVisibility::Hidden);
    BP_OnCrushModeChanged(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// GAS Attribute Callbacks (fired by ASC->SetNumericAttributeBase)
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthWidget::OnLightHealthChanged(const FOnAttributeChangeData& Data)
{
    if (!CachedASC) return;

    const float Max = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxLightHealthAttribute());

    UpdateBar(Data.NewValue, Max);
    TargetBarColor = ResolveTargetBarColor();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("LightHealthWidget: Health changed to %.1f / %.1f (%.0f%%)"),
        Data.NewValue, Max, TargetHealth * 100.f);
#endif
}

void UkdLightHealthWidget::OnMaxLightHealthChanged(const FOnAttributeChangeData& Data)
{
    if (!CachedASC) return;

    const float Current = CachedASC->GetNumericAttribute(UkdAttributeSet::GetLightHealthAttribute());
    UpdateBar(Current, Data.NewValue);
}

// ─────────────────────────────────────────────────────────────────────────────
// Component Callbacks
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthWidget::OnShadowStateChanged(bool bNewInShadow)
{
    bInShadow = bNewInShadow;
    TargetBarColor = ResolveTargetBarColor();

    // ── Light Exposure Warning ─────────────────────────────────────────────
    // Entering light  (bInShadow == false) -> sign appears, blink loop starts.
    // Entering shadow (bInShadow == true)  -> sign hides immediately, blink loop pauses.
    // Reset DangerTimer so the sign is visible right away on the frame the
    // state flips, instead of possibly landing mid-cycle in its "hidden" phase.
    DangerTimer = 0.f;

    if (Txt_LightDanger)
    {
        Txt_LightDanger->SetVisibility(
            bInShadow ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
    }
}

void UkdLightHealthWidget::OnCriticalChanged(bool bNewCritical)
{
    bIsCritical = bNewCritical;
/*    DangerTimer = 0.f;

    if (Txt_LightDanger)
    {
        Txt_LightDanger->SetVisibility(
            bIsCritical ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }*/

    BP_OnCriticalStateChanged(bIsCritical);
}

// ─────────────────────────────────────────────────────────────────────────────
// UpdateBar — single authoritative place that sets both target and display
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthWidget::UpdateBar(float CurrentHealth, float MaxHealth)
{
    TargetHealth = (MaxHealth > 0.f) ? FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f) : 0.f;
    DisplayHealth = TargetHealth;   // snap immediately; tick lerps from here

    if (Bar_LightHealth)
    {
        Bar_LightHealth->SetPercent(DisplayHealth);
    }
}

FLinearColor UkdLightHealthWidget::ResolveTargetBarColor() const
{
    UkdColorTheme* Theme = UkdThemeAccess::GetColorTheme(this);
    if (!Theme)
    {
        // Driver not ready — fall back to the EditDefaults values so the bar
        // is never invisible. These get overwritten on the next state change.
        return bInShadow ? ShadowHealColor : LightDamageColor;
    }

    // Two-state mapping:
    //   In shadow (healing)   → PaleIon  — calm, restorative cool blue
    //   In light (damaging)   → EmberTrace via GetHealthBarColor at low fill,
    //                           Sunmark at high fill (drain reads as warmth lost)
    if (bInShadow)
    {
        return Theme->PaleIon;
    }
    return UkdHUDColorLibrary::GetHealthBarColor(Theme, DisplayHealth);
}

// ─────────────────────────────────────────────────────────────────────────────
// NativeTick — smooth visual polish only; bar is already set by UpdateBar
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
    Super::NativeTick(MyGeometry, DeltaTime);

    TickBarLerp(DeltaTime);
    TickColourLerp(DeltaTime);

    if (!bInShadow)
    {
        TickDangerBlink(DeltaTime);
    }
}

void UkdLightHealthWidget::TickBarLerp(float DeltaTime)
{
    if (!Bar_LightHealth) return;

    DisplayHealth = FMath::FInterpTo(DisplayHealth, TargetHealth, DeltaTime, BarLerpSpeed);
    Bar_LightHealth->SetPercent(DisplayHealth);
}

void UkdLightHealthWidget::TickColourLerp(float DeltaTime)
{
    if (!Bar_LightHealth) return;

    CurrentBarColor = FLinearColor::LerpUsingHSV(
        CurrentBarColor, TargetBarColor,
        FMath::Clamp(DeltaTime * ColourLerpSpeed, 0.f, 1.f));

    Bar_LightHealth->SetFillColorAndOpacity(CurrentBarColor);
}

void UkdLightHealthWidget::TickDangerBlink(float DeltaTime)
{
    if (!Txt_LightDanger) return;

    DangerTimer += DeltaTime;
    if (DangerTimer >= DangerBlinkInterval)
    {
        DangerTimer -= DangerBlinkInterval;

        const ESlateVisibility Current = Txt_LightDanger->GetVisibility();
        Txt_LightDanger->SetVisibility(
            Current == ESlateVisibility::Hidden
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Hidden);
    }
}
