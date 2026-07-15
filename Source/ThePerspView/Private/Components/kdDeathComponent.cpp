// Copyright ASKD Games


#include "Components/kdDeathComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/kdRagdollComponent.h"
#include "Crush/kdCrushTransitionComponent.h"
#include "Crush/kdCrushStateComponent.h"


UkdDeathComponent::UkdDeathComponent()
{

	PrimaryComponentTick.bCanEverTick = false;
}

void UkdDeathComponent::BeginPlay()
{
	Super::BeginPlay();
	
    CachedPlayer = Cast<AkdMyPlayer>(GetOwner());

    if (!CachedPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("UkdDeathComponent: Owner is not AkdMyPlayer — component disabled."));
        return;
    }
}

void UkdDeathComponent::TriggerDeath()
{
    // Guard: only die once per cycle
    if (DeathState != EPlayerDeathState::Alive)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UkdDeathComponent::TriggerDeath called in state %d — ignored."),
            static_cast<int32>(DeathState));
        return;
    }

    // Read momentum FIRST — ShutdownCrushSystems below can zero the CMC.
    const FVector DeathVelocity = CachedPlayer ? CachedPlayer->GetVelocity() : FVector::ZeroVector;

    SetDeathState(EPlayerDeathState::Dying);
    DisablePlayerInput();

    // ── 0. Declare death to GAS ───────────────────────────────────────────────
    // One tag, and the crush mechanic is off everywhere: every ability lists
    // State.Dead in ActivationBlockedTags, UkdJumpSquashComponent self-suspends
    // on it, and UkdPlayerHUDComponent / UkdTutorialSubsystem were already
    // listening for it. Cleared only in ClearAllGASDeathState() on respawn.
    ApplyDeadTag();

    // ── 0b. Stop the crush pipeline. MUST precede EnterRagdoll — the Timeline
    //        writes mesh scale, and Chaos bakes body scale at sim start.
    ShutdownCrushSystems();

    // ── 0c. Ragdoll ───────────────────────────────────────────────────────────
    if (CachedPlayer && CachedPlayer->RagdollComponent)
    {
        CachedPlayer->RagdollComponent->EnterRagdoll(DeathVelocity);
    }

    // ── 1. Red screen-fade ────────────────────────────────────────────────────
    if (APlayerCameraManager* Cam = GetCameraManager())
    {
        Cam->StartCameraFade(
            0.f,                    // from alpha (transparent)
            1.f,                    // to alpha   (opaque)
            DeathFadeDuration,
            DeathFadeColor,
            false,                  // bShouldFadeAudio
            true);                  // bHoldWhenFinished — stays black until respawn
    }

    // ── 2. Slow-motion dip ────────────────────────────────────────────────────
    if (DeathSlowMotionScale < 1.0f)
    {
        UGameplayStatics::SetGlobalTimeDilation(GetWorld(), DeathSlowMotionScale);

        // Restore normal time after half the fade duration so the world isn't
        // permanently slowed while the black screen is held.
        GetWorld()->GetTimerManager().SetTimer(
            SlowMotionRestoreHandle,
            [this]()
            {
                UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
            },
            DeathFadeDuration * 0.5f,
            false);
    }

    // ── 3. Blueprint VFX hook ─────────────────────────────────────────────────
    BP_OnDeathStarted();

    // ── 4. Notify GameMode after fade completes ───────────────────────────────
    GetWorld()->GetTimerManager().SetTimer(
        DeathSequenceTimerHandle,
        this,
        &UkdDeathComponent::FinishDeathSequence,
        DeathFadeDuration,
        false);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("DeathComponent: Death sequence started (fade=%.2fs)."), DeathFadeDuration);
#endif
}

void UkdDeathComponent::TriggerRespawn(const FVector& Location, const FRotator& Rotation)
{
    // Cancel any pending death timers to avoid double-fires on edge cases
    GetWorld()->GetTimerManager().ClearTimer(DeathSequenceTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(SlowMotionRestoreHandle);

    // Guarantee normal time scale in case slow-motion timer hadn't fired yet
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);

    SetDeathState(EPlayerDeathState::Respawning);

    if (!CachedPlayer) return;

    // Defensive: TriggerRespawn is public and documented as safe without a prior
    // TriggerDeath (debug console, forced respawn). Teleporting while the mesh
    // simulates moves the capsule and leaves the corpse behind. Idempotent.
    if (CachedPlayer->RagdollComponent)
    {
        CachedPlayer->RagdollComponent->ExitRagdoll();
    }

    // ── 1. Teleport ───────────────────────────────────────────────────────────
    CachedPlayer->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    CachedPlayer->SetActorRotation(Rotation);

    // ── 2. Reset GAS (tags + stamina) ─────────────────────────────────────────
    ClearAllGASDeathState();
    RestoreStamina();

    // ── 3. Blueprint hook ─────────────────────────────────────────────────────
    BP_OnRespawnStarted();

    // ── 4. Fade back in ───────────────────────────────────────────────────────
    if (APlayerCameraManager* Cam = GetCameraManager())
    {
        Cam->StartCameraFade(
            1.f,                    // from alpha (opaque / black screen)
            0.f,                    // to alpha   (transparent / visible)
            RespawnFadeDuration,
            FLinearColor::Black,
            false,                  // bShouldFadeAudio
            false);                 // bHoldWhenFinished
    }

    // ── 5. Re-enable input then fire delegate after fade ─────────────────────
    EnablePlayerInput();

    GetWorld()->GetTimerManager().SetTimer(
        RespawnCompleteHandle,
        this,
        &UkdDeathComponent::FinishRespawnFade,
        RespawnFadeDuration,
        false);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("DeathComponent: Respawn at %s (fade=%.2fs)."), *Location.ToString(), RespawnFadeDuration);
#endif
}

APlayerCameraManager* UkdDeathComponent::GetCameraManager() const
{
    if (!GetWorld()) return nullptr;
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    return PC ? PC->PlayerCameraManager : nullptr;
}

void UkdDeathComponent::FinishDeathSequence()
{
    SetDeathState(EPlayerDeathState::Dead);

    if (CachedPlayer)
    {
        // Hide FIRST. Everything below is a hard snap that must never be seen —
        // the fade is holding black at this point (bHoldWhenFinished = true).
        CachedPlayer->GetMesh()->SetVisibility(false);

        // Physics off BEFORE we touch the rig: a simulating mesh ignores
        // SetRelativeScale3D and teleports. This ordering is load-bearing.
        if (CachedPlayer->RagdollComponent)
        {
            CachedPlayer->RagdollComponent->ExitRagdoll();
        }

        // World un-folds now, invisibly, during the GameMode's RespawnDelay.
        RestoreWorldToThreeD();
    }

    OnDeathSequenceComplete.Broadcast();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("DeathComponent: Death sequence complete — notifying GameMode."));
#endif
}

void UkdDeathComponent::FinishRespawnFade()
{
    // Restore mesh visibility now that the fade-in has finished and the
    // player is fully back in the world.
    if (CachedPlayer)
    {
        CachedPlayer->GetMesh()->SetVisibility(true);
    }

    SetDeathState(EPlayerDeathState::Alive);
    OnRespawnComplete.Broadcast();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("DeathComponent: Respawn complete — player is Alive."));
#endif
}

void UkdDeathComponent::SetDeathState(EPlayerDeathState NewState)
{
#if !UE_BUILD_SHIPPING
    const UEnum* Enum = StaticEnum<EPlayerDeathState>();
    UE_LOG(LogTemp, Log, TEXT("DeathComponent: %s → %s"),
        *Enum->GetNameStringByValue(static_cast<int64>(DeathState)),
        *Enum->GetNameStringByValue(static_cast<int64>(NewState)));
#endif
    DeathState = NewState;
}

void UkdDeathComponent::DisablePlayerInput()
{
    if (!CachedPlayer) return;

    APlayerController* PC = Cast<APlayerController>(CachedPlayer->GetController());
    if (!PC) return;

    // DisableInput blocks all movement/action bindings while leaving UI input
    // functional so widgets can still receive clicks.
    CachedPlayer->DisableInput(PC);

    // Also flag ignore-move so any queued movement input drains
    PC->SetIgnoreMoveInput(true);
    PC->SetIgnoreLookInput(true);
}

void UkdDeathComponent::EnablePlayerInput()
{
    if (!CachedPlayer) return;

    APlayerController* PC = Cast<APlayerController>(CachedPlayer->GetController());
    if (!PC) return;

    CachedPlayer->EnableInput(PC);
    PC->ResetIgnoreMoveInput();
    PC->ResetIgnoreLookInput();
}

void UkdDeathComponent::ClearAllGASDeathState()
{
    if (!CachedPlayer) return;

    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return;

    const FkdGameplayTags& StateTags = FkdGameplayTags::Get();

    // Remove every tag that death/crush-mode could have left behind
    const TArray<FGameplayTag> TagsToRemove =
    {
        StateTags.State_Exhausted,
        StateTags.State_CrushMode,
        StateTags.State_InShadow,
        StateTags.State_Transitioning,
        StateTags.State_Dashing,
        StateTags.State_EnemyContact,
        StateTags.State_Dead,
    };

    for (const FGameplayTag& Tag : TagsToRemove)
    {
        if (ASC->HasMatchingGameplayTag(Tag))
        {
            ASC->RemoveLooseGameplayTag(Tag);
        }
    }

    // Cancel all active abilities (drain, dash cooldown, crush toggle, etc.)
    ASC->CancelAllAbilities();

    // Remove all active gameplay effects so drain/regen don't persist
    FGameplayEffectQuery Query;
    ASC->RemoveActiveEffects(Query);
    

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("DeathComponent: GAS state cleared on respawn."));
#endif
}

void UkdDeathComponent::RestoreStamina()
{
    if (!CachedPlayer) return;

    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return;

    const float MaxStamina = ASC->GetNumericAttribute(
        UkdAttributeSet::GetMaxShadowStaminaAttribute());

    const float RestoredStamina = MaxStamina * FMath::Clamp(RespawnStaminaPercent, 0.f, 1.f);

    // Direct attribute set — bypass effects so there's no drain race condition
    if (UkdAttributeSet* AttrSet = const_cast<UkdAttributeSet*>(
        Cast<UkdAttributeSet>(ASC->GetAttributeSet(UkdAttributeSet::StaticClass()))))
    {
        AttrSet->SetShadowStamina(RestoredStamina);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("DeathComponent: Stamina restored to %.0f / %.0f"),
            RestoredStamina, MaxStamina);
#endif
    }
}

void UkdDeathComponent::ApplyDeadTag()
{
    if (!CachedPlayer) return;
    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return;

    const FGameplayTag& Dead = FkdGameplayTags::Get().State_Dead;
    if (!ASC->HasMatchingGameplayTag(Dead))
    {
        ASC->AddLooseGameplayTag(Dead);
    }
}

void UkdDeathComponent::ShutdownCrushSystems()
{
    if (!CachedPlayer) return;
    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return;

    // Cancel first: each ability's EndAbility does its own teardown — the crush
    // toggle removes State.Transitioning, unbinds OnTransitionComplete, and
    // strips the drain effect. Aborting the timeline BEFORE cancelling would let
    // the ability's OnTransitionFinished run against a half-morph.
    ASC->CancelAllAbilities();

    // The Timeline is owned by the component, not the ability, so the cancel
    // above does not stop it. Without this it keeps writing mesh scale + camera
    // FOV over the ragdoll for up to 0.63s.
    if (CachedPlayer->CrushTransitionComponent)
    {
        CachedPlayer->CrushTransitionComponent->AbortTransition();
    }

    // ── Why State.CrushMode stays ON here ────────────────────────────────────
    // UkdGeometryTransitionComponent, UkdWorldColorDriver, UkdLightHealthComponent
    // and UkdGameFeedbackComponent all un-fold the world on that tag dropping.
    // Sliding world geometry through a simulating ragdoll is a guaranteed
    // penetration blow-up. The un-crush happens in FinishDeathSequence(), under
    // the black screen, after ExitRagdoll — and it reads better: you die crushed.
}

void UkdDeathComponent::RestoreWorldToThreeD()
{
    if (!CachedPlayer) return;
    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return;

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    if (ASC->HasMatchingGameplayTag(Tags.State_CrushMode))
    {
        // Owned by CrushStateComponent: releases the plane constraint, projects
        // velocity off the collapse axis, drops State.InShadow.
        if (CachedPlayer->CrushStateComponent)
        {
            CachedPlayer->CrushStateComponent->ToggleShadowTracking(false);
        }

        // This is the signal the whole world listens to. AkdMyPlayer's new
        // State.Dead guard stops it answering with a morph.
        ASC->RemoveLooseGameplayTag(Tags.State_CrushMode);
    }

    // Unconditional: a death timed into a morph can leave the camera at 14° FOV
    // with a 2200cm arm even if State.CrushMode was never applied.
    if (CachedPlayer->CrushTransitionComponent)
    {
        CachedPlayer->CrushTransitionComponent->SnapToThreeD();
    }
}
