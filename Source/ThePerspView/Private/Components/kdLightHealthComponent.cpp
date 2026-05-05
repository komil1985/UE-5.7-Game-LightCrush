// Copyright ASKD Games

#include "Components/kdLightHealthComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameMode/kdGameModeBase.h"
#include "Components/kdDeathComponent.h"
#include "AbilitySystemComponent.h"



UkdLightHealthComponent::UkdLightHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    CachedPlayer = Cast<AkdMyPlayer>(GetOwner());
    if (!CachedPlayer)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("LightHealthComponent [%s]: Owner is not AkdMyPlayer — disabled."),
            *GetName());
        return;
    }

    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("LightHealthComponent: No AbilitySystemComponent found — disabled."));
        return;
    }

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    // ── CrushMode tag → start / stop the health tick ──────────────────────────
    ASC->RegisterGameplayTagEvent(Tags.State_CrushMode, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UkdLightHealthComponent::OnCrushModeTagChanged);

    // ── InShadow tag → switch between heal and damage mode ────────────────────
    ASC->RegisterGameplayTagEvent(Tags.State_InShadow, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UkdLightHealthComponent::OnInShadowTagChanged);

    // ── Respawn → restore health automatically ────────────────────────────────
    // Binds to the DeathComponent's delegate so we need zero coupling with
    // AkdGameModeBase or UkdDeathComponent internals.
    if (CachedPlayer->DeathComponent)
    {
        CachedPlayer->DeathComponent->OnRespawnComplete.AddDynamic(
            this, &UkdLightHealthComponent::OnRespawnComplete);
    }

    // ── Sync initial state if level starts already in CrushMode ───────────────
    if (ASC->HasMatchingGameplayTag(Tags.State_CrushMode))
    {
        bIsInShadow = ASC->HasMatchingGameplayTag(Tags.State_InShadow);
        StartHealthTick();
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("LightHealthComponent: Initialised — MaxHealth=%.0f"),
        GetMaxHealth());
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Tag Callbacks
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthComponent::OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        // Entered CrushMode — sync shadow state then start ticking
        if (UAbilitySystemComponent* ASC = CachedPlayer ? CachedPlayer->GetAbilitySystemComponent() : nullptr)
        {
            const bool bNewShadow = ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_InShadow);
            if (bNewShadow != bIsInShadow)
            {
                bIsInShadow = bNewShadow;
                OnShadowStateChanged.Broadcast(bIsInShadow);
            }
        }
        StartHealthTick();
    }
    else
    {
        // Exited CrushMode — stop ticking (no damage / heal in 3D mode)
        StopHealthTick();
    }
}

void UkdLightHealthComponent::OnInShadowTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    const bool bNewShadow = (NewCount > 0);
    if (bNewShadow == bIsInShadow) return;  // no change, skip broadcast

    bIsInShadow = bNewShadow;
    OnShadowStateChanged.Broadcast(bIsInShadow);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("LightHealthComponent: InShadow=%s"),
        bIsInShadow ? TEXT("true") : TEXT("false"));
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Health Tick
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthComponent::StartHealthTick()
{
    UWorld* World = GetWorld();
    if (World->GetTimerManager().IsTimerActive(HealthTickHandle)) return;

    World->GetTimerManager().SetTimer(
        HealthTickHandle,
        this,
        &UkdLightHealthComponent::HealthTick,
        HealthTickInterval,
        true   // repeating
    );
}

void UkdLightHealthComponent::StopHealthTick()
{
    UWorld* World = GetWorld();
    World->GetTimerManager().ClearTimer(HealthTickHandle);
}

void UkdLightHealthComponent::HealthTick()
{
    if (!CachedPlayer) return;

    // Positive delta = healing (shadow), negative = damage (light)
    const float Delta = bIsInShadow
        ? (ShadowHealRate * HealthTickInterval)
        : -(LightDamageRate * HealthTickInterval);

    const float Current = GetCurrentHealth();
    const float Max = GetMaxHealth();

    if (Max <= 0.f) return;  // attribute not yet initialised

    const float NewVal = FMath::Clamp(Current + Delta, 0.f, Max);
    SetCurrentHealth(NewVal);

    // Broadcast to UI
    OnLightHealthChanged.Broadcast(NewVal, Max);

    // ── Critical threshold transition ─────────────────────────────────────────
    const bool bNowCritical = (NewVal / Max) < CriticalHealthThreshold;
    if (bNowCritical != bWasCritical)
    {
        bWasCritical = bNowCritical;
        OnLightCriticalChanged.Broadcast(bNowCritical);
    }

    // ── Death on depletion ────────────────────────────────────────────────────
    if (NewVal <= 0.f)
    {
        StopHealthTick();
        TriggerLightDeath();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Attribute Helpers
// ─────────────────────────────────────────────────────────────────────────────

float UkdLightHealthComponent::GetCurrentHealth() const
{
    if (!CachedPlayer) return 0.f;
    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return 0.f;
    return ASC->GetNumericAttribute(UkdAttributeSet::GetLightHealthAttribute());
}

float UkdLightHealthComponent::GetMaxHealth() const
{
    if (!CachedPlayer) return 0.f;
    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return 0.f;
    return ASC->GetNumericAttribute(UkdAttributeSet::GetMaxLightHealthAttribute());
}

void UkdLightHealthComponent::SetCurrentHealth(float NewValue)
{
    if (!CachedPlayer) return;
    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return;

    const UAttributeSet* BaseSet = ASC->GetAttributeSet(UkdAttributeSet::StaticClass());
    if (!BaseSet) return;

    UkdAttributeSet* AttrSet = const_cast<UkdAttributeSet*>(Cast<UkdAttributeSet>(BaseSet));
    if (AttrSet)
    {
        AttrSet->SetLightHealth(NewValue);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// State Queries
// ─────────────────────────────────────────────────────────────────────────────

bool UkdLightHealthComponent::IsCritical() const
{
    const float Max = GetMaxHealth();
    if (Max <= 0.f) return false;
    return (GetCurrentHealth() / Max) < CriticalHealthThreshold;
}

float UkdLightHealthComponent::GetHealthPercent() const
{
    const float Max = GetMaxHealth();
    if (Max <= 0.f) return 0.f;
    return FMath::Clamp(GetCurrentHealth() / Max, 0.f, 1.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Respawn
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthComponent::OnRespawnComplete()
{
    RestoreLightHealth();
}

void UkdLightHealthComponent::RestoreLightHealth()
{
    const float Max = GetMaxHealth();
    if (Max <= 0.f) return;

    SetCurrentHealth(Max);
    bWasCritical = false;

    OnLightHealthChanged.Broadcast(Max, Max);
    OnLightCriticalChanged.Broadcast(false);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("LightHealthComponent: Restored to %.0f"), Max);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Death Trigger
// ─────────────────────────────────────────────────────────────────────────────

void UkdLightHealthComponent::TriggerLightDeath()
{
    if (!CachedPlayer) return;

    // Guard against double-death — defer to DeathComponent's own guard
    if (CachedPlayer->DeathComponent && !CachedPlayer->DeathComponent->IsAlive()) return;

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning,
        TEXT("LightHealthComponent: LightHealth depleted — triggering death."));
#endif

    if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        GM->HandlePlayerDeath();
    }
}
