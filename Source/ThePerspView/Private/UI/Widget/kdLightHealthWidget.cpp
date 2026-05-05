// Copyright ASKD Games


#include "UI/Widget/kdLightHealthWidget.h"
#include "Components/kdLightHealthComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Kismet/GameplayStatics.h"


void UkdLightHealthWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Sensible defaults before InitializeWithComponent is called
    CurrentBarColor = ShadowHealColor;
    TargetBarColor = ShadowHealColor;

    if (Bar_LightHealth)
    {
        Bar_LightHealth->SetPercent(1.f);
        Bar_LightHealth->SetFillColorAndOpacity(CurrentBarColor);
    }

    if (Txt_LightDanger)
    {
        Txt_LightDanger->SetVisibility(ESlateVisibility::Hidden);
    }
}

void UkdLightHealthWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
    Super::NativeTick(MyGeometry, DeltaTime);

    if (!bInCrushMode) return;

    TickBarLerp(DeltaTime);
    TickColourLerp(DeltaTime);

    if (bIsCritical)
    {
        TickDangerBlink(DeltaTime);
    }
}

void UkdLightHealthWidget::NativeDestruct()
{
    // Remove delegate handles to prevent dangling references
    if (CachedComponent)
    {
        CachedComponent->OnLightHealthChanged.Remove(HealthChangedHandle);
        CachedComponent->OnLightCriticalChanged.Remove(CriticalChangedHandle);
        CachedComponent->OnShadowStateChanged.Remove(ShadowStateChangedHandle);
    }

    Super::NativeDestruct();
}

void UkdLightHealthWidget::InitializeWithComponent(UkdLightHealthComponent* InComponent)
{
    if (!InComponent) return;
    CachedComponent = InComponent;

    // ── Bind component delegates ──────────────────────────────────────────────
    HealthChangedHandle = CachedComponent->OnLightHealthChanged.AddUObject(
        this, &UkdLightHealthWidget::OnHealthChanged);

    CriticalChangedHandle = CachedComponent->OnLightCriticalChanged.AddUObject(
        this, &UkdLightHealthWidget::OnCriticalChanged);

    ShadowStateChangedHandle = CachedComponent->OnShadowStateChanged.AddUObject(
        this, &UkdLightHealthWidget::OnShadowStateChanged);

    // ── Register CrushMode tag → show / hide the widget ──────────────────────
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(
        UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

    if (Player)
    {
        if (UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent())
        {
            ASC->RegisterGameplayTagEvent(
                FkdGameplayTags::Get().State_CrushMode,
                EGameplayTagEventType::NewOrRemoved)
                .AddUObject(this, &UkdLightHealthWidget::OnCrushModeTagChanged);

            // Sync immediately if already in CrushMode (e.g. BeginPlay hot-reload)
            bInCrushMode = ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode);

            // Also sync shadow state immediately
            bInShadow = ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_InShadow);
        }
    }

    // ── Sync initial display values from component ────────────────────────────
    //bInShadow = InComponent->GetHealthPercent() >= 0.f;  // shadow state comes via tag
    TargetHealth = InComponent->GetHealthPercent();
    DisplayHealth = TargetHealth;
    TargetBarColor = bInShadow ? ShadowHealColor : LightDamageColor;
    CurrentBarColor = TargetBarColor;

    if (Bar_LightHealth)
    {
        Bar_LightHealth->SetPercent(DisplayHealth);
        Bar_LightHealth->SetFillColorAndOpacity(CurrentBarColor);
    }

    //// Hide the widget until CrushMode is active
    //if (!bInCrushMode && Overlay_Root)
    //{
    //    Overlay_Root->SetVisibility(ESlateVisibility::Hidden);
    //}
}

void UkdLightHealthWidget::OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    bInCrushMode = (NewCount > 0);

    BP_OnCrushModeChanged(bInCrushMode);

    SetVisibility(bInCrushMode ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);

    // Fallback show/hide if no Blueprint animation is authored
    if (Overlay_Root)
    {
        Overlay_Root->SetVisibility(bInCrushMode ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }

    // Sync shadow state when entering crush mode
    if (bInCrushMode)
    {
        if (CachedComponent)
        {
            bInShadow = CachedComponent->GetHealthPercent() >= 0.f; // placeholder
        }

        // Re-sync the bar immediately so it shows the current value
        if (CachedComponent && Bar_LightHealth)
        {
            TargetHealth = CachedComponent->GetHealthPercent();
            DisplayHealth = TargetHealth;
            Bar_LightHealth->SetPercent(DisplayHealth);
        }
    }
}

void UkdLightHealthWidget::OnHealthChanged(float Current, float Max)
{
    TargetHealth = (Max > 0.f) ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f;

    DisplayHealth = TargetHealth;

    if (Bar_LightHealth)
    {
        Bar_LightHealth->SetPercent(DisplayHealth);
        Bar_LightHealth->SetFillColorAndOpacity(CurrentBarColor);
    }
}

void UkdLightHealthWidget::OnCriticalChanged(bool bNewCritical)
{
    bIsCritical = bNewCritical;
    DangerTimer = 0.f;

    if (Txt_LightDanger)
    {
        Txt_LightDanger->SetVisibility(bIsCritical ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }

    BP_OnCriticalStateChanged(bIsCritical);
}

void UkdLightHealthWidget::OnShadowStateChanged(bool bNewInShadow)
{
    bInShadow = bNewInShadow;
    TargetBarColor = bInShadow ? ShadowHealColor : LightDamageColor;
    CurrentBarColor = TargetBarColor;   // snap colour immediately so bar always matches state

    if (Bar_LightHealth)
    {
        Bar_LightHealth->SetFillColorAndOpacity(CurrentBarColor);
    }
}

void UkdLightHealthWidget::TickBarLerp(float DeltaTime)
{
    if (!Bar_LightHealth) return;

    // Smooth interpolation so the bar animates rather than snapping
    DisplayHealth = FMath::FInterpTo(DisplayHealth, TargetHealth, DeltaTime, BarLerpSpeed);
    Bar_LightHealth->SetPercent(DisplayHealth);
}

void UkdLightHealthWidget::TickColourLerp(float DeltaTime)
{
    if (!Bar_LightHealth) return;

    // Lerp R, G, B, A individually (FLinearColor::LerpUsingHSV gives nicer transitions)
    CurrentBarColor = FLinearColor::LerpUsingHSV(CurrentBarColor, TargetBarColor,
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

        // Toggle visibility — creates a fast blink effect without animation tracks
        const ESlateVisibility Current = Txt_LightDanger->GetVisibility();
        Txt_LightDanger->SetVisibility(
            Current == ESlateVisibility::Hidden
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Hidden);
    }
}
