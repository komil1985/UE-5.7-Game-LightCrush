// Copyright ASKD Games


#include "Components/kdPlayerHUDComponent.h"
#include "UI/Widget/kdHUDWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"


UkdPlayerHUDComponent::UkdPlayerHUDComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UkdPlayerHUDComponent::BeginPlay()
{
    Super::BeginPlay();

    //EnsureWidget();

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
    bUpdatesFrozen = false;

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

    // Death → freeze the bars. Mirrors the CrushMode tag-event pattern.
    DeadTagHandle = ASC->RegisterGameplayTagEvent(
        FkdGameplayTags::Get().State_Dead,
        EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UkdPlayerHUDComponent::OnDeadTagChanged);

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
        BoundASC->UnregisterGameplayTagEvent(DeadTagHandle, FkdGameplayTags::Get().State_Dead, EGameplayTagEventType::NewOrRemoved);
    }

    HealthChangedHandle.Reset();
    MaxHealthChangedHandle.Reset();
    StaminaChangedHandle.Reset();
    MaxStaminaChangedHandle.Reset();
    DeadTagHandle.Reset();

    BoundASC = nullptr;
}

void UkdPlayerHUDComponent::HandleHealthChanged(const FOnAttributeChangeData& Data) 
{ 
    if (bUpdatesFrozen) return;
    CachedHealth = Data.NewValue;     
    RefreshHealthBar(); 
}

void UkdPlayerHUDComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& Data) 
{ 
    CachedMaxHealth = Data.NewValue;  
    RefreshHealthBar();
}

void UkdPlayerHUDComponent::HandleStaminaChanged(const FOnAttributeChangeData& Data) 
{ 
    CachedStamina = Data.NewValue;   
    RefreshStaminaBar(); 
}

void UkdPlayerHUDComponent::HandleMaxStaminaChanged(const FOnAttributeChangeData& Data) 
{ 
    CachedMaxStamina = Data.NewValue; 
    RefreshStaminaBar(); 
}

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

void UkdPlayerHUDComponent::FreezeUpdates()
{
    bUpdatesFrozen = true;
}

void UkdPlayerHUDComponent::UnfreezeUpdates()
{
    bUpdatesFrozen = false;
    if (!IsValid(BoundASC)) return;

    // Re-seed to current ASC values so the bars catch up after being frozen.
    CachedHealth = BoundASC->GetNumericAttribute(UkdAttributeSet::GetLightHealthAttribute());
    CachedMaxHealth = BoundASC->GetNumericAttribute(UkdAttributeSet::GetMaxLightHealthAttribute());
    CachedStamina = BoundASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());
    CachedMaxStamina = BoundASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
    RefreshHealthBar();
    RefreshStaminaBar();
}

void UkdPlayerHUDComponent::OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    if (NewCount > 0)
    {
        // Tag added → player is dead → lock the bars at their last values.
        FreezeUpdates();
    }
    // NewCount == 0 (tag removed, e.g. on respawn) is intentionally NOT auto-unfrozen
    // here — a fresh possession runs InitializeForPawn(), which resets bUpdatesFrozen.
    // If you respawn WITHOUT re-possessing, add: else { UnfreezeUpdates(); }
}
