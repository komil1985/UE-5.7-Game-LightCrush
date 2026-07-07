// Copyright ASKD Games


#include "Components/kdPlayerHUDComponent.h"
#include "UI/Widget/kdHUDWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// TODO(adapt): point this at your actual AttributeSet header + class name.
//   The four accessors below (Get<Attr>Attribute) are the standard ATTRIBUTE_ACCESSORS
//   boilerplate — verify they exist / match your naming.
#include "AbilitySystem/kdAttributeSet.h"

UkdPlayerHUDComponent::UkdPlayerHUDComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UkdPlayerHUDComponent::BeginPlay()
{
    Super::BeginPlay();

    EnsureWidget();

    // If possession happened before BeginPlay, bind immediately (covers both orderings).
    if (APlayerController* PC = GetOwningPlayerController())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            InitializeForPawn(Pawn);
        }
    }
}

void UkdPlayerHUDComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleasePawn();
    DestroyWidget();
    Super::EndPlay(EndPlayReason);
}

APlayerController* UkdPlayerHUDComponent::GetOwningPlayerController() const
{
    return Cast<APlayerController>(GetOwner());
}

void UkdPlayerHUDComponent::EnsureWidget()
{
    if (IsValid(HUDWidget))
    {
        return;
    }

    APlayerController* PC = GetOwningPlayerController();
    if (!PC || !PC->IsLocalController())   // only local controllers own a viewport HUD
    {
        return;
    }

    if (!HUDWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[kdPlayerHUDComponent] HUDWidgetClass unset; no HUD created."));
        return;
    }

    HUDWidget = CreateWidget<UkdHUDWidget>(PC, HUDWidgetClass);
    if (IsValid(HUDWidget))
    {
        HUDWidget->AddToViewport(ViewportZOrder);
    }
}

void UkdPlayerHUDComponent::DestroyWidget()
{
    if (IsValid(HUDWidget))
    {
        HUDWidget->RemoveFromParent();   // UE5: RemoveFromViewport is deprecated
    }
    HUDWidget = nullptr;
}

void UkdPlayerHUDComponent::SetHUDVisible(bool bVisible)
{
    if (IsValid(HUDWidget))
    {
        HUDWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }
}

void UkdPlayerHUDComponent::InitializeForPawn(APawn* InPawn)
{
    ReleasePawn();   // idempotent: drop any previous binding first

    if (!InPawn)
    {
        return;
    }

    // Resolves via IAbilitySystemInterface. If your ASC lives on the PlayerState
    // instead of the pawn, pass InPawn->GetPlayerState() here.
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InPawn);
    if (!ASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[kdPlayerHUDComponent] Possessed pawn has no ASC; HUD shows defaults."));
        return;
    }

    BoundASC = ASC;

    // GetGameplayAttributeValueChangeDelegate fires on base-value writes made via
    // SetNumericAttributeBase and on aggregated current-value changes.
    HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        UkdAttributeSet::GetLightHealthAttribute())
        .AddUObject(this, &UkdPlayerHUDComponent::HandleHealthChanged);

    MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        UkdAttributeSet::GetMaxLightHealthAttribute())
        .AddUObject(this, &UkdPlayerHUDComponent::HandleMaxHealthChanged);

    StaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        UkdAttributeSet::GetShadowStaminaAttribute())
        .AddUObject(this, &UkdPlayerHUDComponent::HandleStaminaChanged);

    MaxStaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        UkdAttributeSet::GetMaxShadowStaminaAttribute())
        .AddUObject(this, &UkdPlayerHUDComponent::HandleMaxStaminaChanged);

    // Seed so the bars are correct on the first frame.
    CachedHealth = ASC->GetNumericAttribute(UkdAttributeSet::GetLightHealthAttribute());
    CachedMaxHealth = ASC->GetNumericAttribute(UkdAttributeSet::GetMaxLightHealthAttribute());
    CachedStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());
    CachedMaxStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());

    EnsureWidget();   // in case possession preceded viewport readiness
    RefreshHealthBar();
    RefreshStaminaBar();
}

void UkdPlayerHUDComponent::ReleasePawn()
{
    if (IsValid(BoundASC))
    {
        BoundASC->GetGameplayAttributeValueChangeDelegate(UkdAttributeSet::GetLightHealthAttribute()).Remove(HealthChangedHandle);
        BoundASC->GetGameplayAttributeValueChangeDelegate(UkdAttributeSet::GetMaxLightHealthAttribute()).Remove(MaxHealthChangedHandle);
        BoundASC->GetGameplayAttributeValueChangeDelegate(UkdAttributeSet::GetShadowStaminaAttribute()).Remove(StaminaChangedHandle);
        BoundASC->GetGameplayAttributeValueChangeDelegate(UkdAttributeSet::GetMaxShadowStaminaAttribute()).Remove(MaxStaminaChangedHandle);
    }

    HealthChangedHandle.Reset();
    MaxHealthChangedHandle.Reset();
    StaminaChangedHandle.Reset();
    MaxStaminaChangedHandle.Reset();

    BoundASC = nullptr;
}

void UkdPlayerHUDComponent::HandleHealthChanged(const FOnAttributeChangeData& Data) { CachedHealth = Data.NewValue;     RefreshHealthBar(); }
void UkdPlayerHUDComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& Data) { CachedMaxHealth = Data.NewValue;  RefreshHealthBar(); }
void UkdPlayerHUDComponent::HandleStaminaChanged(const FOnAttributeChangeData& Data) { CachedStamina = Data.NewValue;    RefreshStaminaBar(); }
void UkdPlayerHUDComponent::HandleMaxStaminaChanged(const FOnAttributeChangeData& Data) { CachedMaxStamina = Data.NewValue; RefreshStaminaBar(); }

void UkdPlayerHUDComponent::RefreshHealthBar() const
{
    if (!IsValid(HUDWidget)) { return; }
    const float Pct = (CachedMaxHealth > KINDA_SMALL_NUMBER) ? (CachedHealth / CachedMaxHealth) : 0.f;
    HUDWidget->SetHealthPercent(Pct);
}

void UkdPlayerHUDComponent::RefreshStaminaBar() const
{
    if (!IsValid(HUDWidget)) { return; }
    const float Pct = (CachedMaxStamina > KINDA_SMALL_NUMBER) ? (CachedStamina / CachedMaxStamina) : 0.f;
    HUDWidget->SetStaminaPercent(Pct);
}

