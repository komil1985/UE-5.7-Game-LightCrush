// Copyright ASKD Games

#include "Subsystem/kdLevelIntroSubsystem.h"
#include "Settings/kdLevelIntroSettings.h"
#include "World/kdLevelGoal.h"
#include "Player/kdMyPlayer.h"
#include "Player/kdPlayerController.h"
#include "GameMode/kdGameModeBase.h"
#include "GameInstance/kdGameInstance.h"
#include "Components/kdWorldColorDriver.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogkdLevelIntro, Log, All);

namespace
{
    // The pawn is possessed before world BeginPlay in every normal load path;
    // this is only a safety net so we never tick forever.
    constexpr int32 kMaxWaitTicks = 120;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool UkdLevelIntroSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer)) return false;
    const UWorld* W = Cast<UWorld>(Outer);
    return W && W->IsGameWorld() && !W->IsNetMode(NM_DedicatedServer);
}

void UkdLevelIntroSubsystem::Deinitialize()
{
    // Level travel / PIE end: release input + timer holds, but do NOT touch the
    // view target — the world is going away.
    ReleaseIntroResources();
    Phase = EPhase::Finished;
    Super::Deinitialize();
}

UkdLevelIntroSubsystem* UkdLevelIntroSubsystem::Get(const UObject* WorldContext)
{
    if (!GEngine || !WorldContext) return nullptr;
    const UWorld* W = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
    return W ? W->GetSubsystem<UkdLevelIntroSubsystem>() : nullptr;
}

bool UkdLevelIntroSubsystem::IsTickable() const
{
    return IsInitialized()
        && (Phase == EPhase::WaitingForPawn || Phase == EPhase::Holding || Phase == EPhase::Blending);
}

TStatId UkdLevelIntroSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UkdLevelIntroSubsystem, STATGROUP_Tickables);
}

bool UkdLevelIntroSubsystem::IsIntroActive() const
{
    return Phase == EPhase::Holding || Phase == EPhase::Blending;
}

// ─── Entry point ──────────────────────────────────────────────────────────────

void UkdLevelIntroSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    const UkdLevelIntroSettings* S = GetDefault<UkdLevelIntroSettings>();
    if (!S || !S->bEnabled)
    {
        Phase = EPhase::Disabled;
        return;
    }

    // Only gameplay levels use AkdGameModeBase. The main menu (AkdMainMenuGameMode)
    // and any non-kd map are excluded by this single check.
    if (!Cast<AkdGameModeBase>(InWorld.GetAuthGameMode()))
    {
        Phase = EPhase::Disabled;
        return;
    }

    // Bare map name — strips "UEDPIE_0_" in PIE, same as the audio / tutorial routes.
    FString MapStr = InWorld.GetMapName();
    MapStr.RemoveFromStart(InWorld.StreamingLevelsPrefix);
    LevelName = FName(*MapStr);

    if (S->ExcludedLevels.Contains(LevelName))
    {
        UE_LOG(LogkdLevelIntro, Log, TEXT("[LevelIntro] '%s' is in ExcludedLevels — skipped."), *LevelName.ToString());
        Phase = EPhase::Disabled;
        return;
    }

    if (UkdGameInstance* GI = InWorld.GetGameInstance<UkdGameInstance>())
    {
        if (LevelName == GI->MainMenuLevelName ||
            (S->bSkipInTutorial && LevelName == GI->TutorialLevelName))
        {
            Phase = EPhase::Disabled;
            return;
        }

        if (S->bOncePerSession && GI->HasPlayedLevelIntro(LevelName))
        {
            UE_LOG(LogkdLevelIntro, Log, TEXT("[LevelIntro] '%s' already played this session — skipped."), *LevelName.ToString());
            Phase = EPhase::Disabled;
            return;
        }
    }

    Phase = EPhase::WaitingForPawn;
    WaitTicks = 0;

    // Try right now: starting synchronously means the very first rendered frame
    // is already the goal shot (no one-frame flash of the player's view).
    TryBegin();
}

AkdLevelGoal* UkdLevelIntroSubsystem::FindGoal() const
{
    AkdLevelGoal* Found = nullptr;
    int32 Count = 0;

    for (TActorIterator<AkdLevelGoal> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It)) continue;
        if (!Found) Found = *It;
        ++Count;
    }

    if (Count > 1)
    {
        UE_LOG(LogkdLevelIntro, Warning,
            TEXT("[LevelIntro] %d AkdLevelGoal actors found — framing '%s' only."),
            Count, *GetNameSafe(Found));
    }
    return Found;
}

void UkdLevelIntroSubsystem::TryBegin()
{
    UWorld* World = GetWorld();
    const UkdLevelIntroSettings* S = GetDefault<UkdLevelIntroSettings>();
    if (!World || !S) { Phase = EPhase::Finished; return; }

    APlayerController* PC = World->GetFirstPlayerController();
    AkdMyPlayer* Player = PC ? Cast<AkdMyPlayer>(PC->GetPawn()) : nullptr;
    if (!PC || !Player || !PC->PlayerCameraManager)
    {
        return;   // stay in WaitingForPawn; Tick retries
    }

    AkdLevelGoal* Goal = FindGoal();
    if (!Goal)
    {
        UE_LOG(LogkdLevelIntro, Log, TEXT("[LevelIntro] No AkdLevelGoal in '%s' — nothing to reveal."), *LevelName.ToString());
        Phase = EPhase::Finished;
        return;
    }

    // ── Reproduce the player's camera geometry, centred on the goal ───────────
    // Same yaw (control rotation), same pitch (arm), same arm length, same FOV, so
    // the goal is framed exactly the way the player normally sees themselves and
    // the later blend is almost a pure translation.
    const USpringArmComponent* Arm = Player->SpringArm;
    const float ArmLen = Arm ? Arm->TargetArmLength : 1000.f;
    const float Pitch = (Arm && !Arm->bInheritPitch) ? Arm->GetRelativeRotation().Pitch : PC->GetControlRotation().Pitch;
    const float Fov = Player->Camera ? Player->Camera->FieldOfView : 95.f;

    // Mirror the player camera's aspect-ratio setup exactly, so the intro shot is
    // framed (and letterboxed, if at all) identically to normal gameplay.
    const bool  bConstrainAspect = Player->Camera ? (Player->Camera->bConstrainAspectRatio != 0) : false;
    const float AspectRatio = Player->Camera ? Player->Camera->AspectRatio : (16.f / 9.f);

    const FRotator CamRot(Pitch, PC->GetControlRotation().Yaw, 0.f);
    const FVector  Focus = Goal->GetActorLocation() + S->GoalFocusOffset;
    FVector CamLoc = Focus - CamRot.Vector() * ArmLen;

    // Pull in if a wall is between the goal and the ideal camera spot — the same
    // thing the spring arm's probe does for the live camera.
    {
        FCollisionQueryParams Params(SCENE_QUERY_STAT(kdLevelIntroProbe), /*bTraceComplex*/ false, Goal);
        Params.AddIgnoredActor(Player);

        FHitResult Hit;
        const bool bHit = World->SweepSingleByChannel(
            Hit, Focus, CamLoc, FQuat::Identity, ECC_Camera,
            FCollisionShape::MakeSphere(S->ProbeRadius), Params);

        // A start-penetrating hit (goal embedded in geometry) would put the camera
        // AT the goal — ignore it and keep the ideal position.
        if (bHit && Hit.bBlockingHit && !Hit.bStartPenetrating)
        {
            CamLoc = Hit.Location;
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.ObjectFlags |= RF_Transient;   // never saved into the level
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    IntroCamera = World->SpawnActor<ACameraActor>(CamLoc, CamRot, SpawnParams);
    if (!IntroCamera)
    {
        UE_LOG(LogkdLevelIntro, Error, TEXT("[LevelIntro] Failed to spawn intro camera."));
        Phase = EPhase::Finished;
        return;
    }

    IntroCamera->SetActorEnableCollision(false);
    if (UCameraComponent* CamComp = IntroCamera->GetCameraComponent())
    {
        CamComp->SetFieldOfView(Fov);
        CamComp->SetConstraintAspectRatio(bConstrainAspect);
        CamComp->SetAspectRatio(AspectRatio);
    }

    CachedPC = PC;
    CachedPlayer = Player;
    HoldStartLoc = CamLoc;
    HoldDir = CamRot.Vector();
    PhaseTime = 0.f;

    // ── Take over the view, block input, pause the level timer ────────────────
    PC->SetViewTargetWithBlend(IntroCamera, 0.f);   // instant cut — no blend on the way IN

    if (S->FadeInSeconds > KINDA_SMALL_NUMBER)
    {
        PC->PlayerCameraManager->StartCameraFade(
            1.f, 0.f, S->FadeInSeconds, FLinearColor::Black, /*bShouldFadeAudio*/ false, /*bHoldWhenFinished*/ false);
    }

    if (AkdPlayerController* KPC = Cast<AkdPlayerController>(PC))
    {
        KPC->SetGameplayInputBlocked(true);
    }
    if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(World->GetAuthGameMode()))
    {
        GM->SetLevelTimerSuspended(true);
    }
    if (UkdGameInstance* GI = World->GetGameInstance<UkdGameInstance>())
    {
        GI->MarkLevelIntroPlayed(LevelName);
    }

    Phase = EPhase::Holding;

    UE_LOG(LogkdLevelIntro, Log,
        TEXT("[LevelIntro] BEGIN '%s' | goal=%s | cam=%s | hold=%.2fs blend=%.2fs"),
        *LevelName.ToString(), *GetNameSafe(Goal), *CamLoc.ToString(), S->HoldSeconds, S->BlendSeconds);
}

// ─── Tick ─────────────────────────────────────────────────────────────────────

void UkdLevelIntroSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const UkdLevelIntroSettings* S = GetDefault<UkdLevelIntroSettings>();
    if (!S) { Phase = EPhase::Finished; return; }

    switch (Phase)
    {
    case EPhase::WaitingForPawn:
        if (++WaitTicks > kMaxWaitTicks)
        {
            UE_LOG(LogkdLevelIntro, Warning, TEXT("[LevelIntro] Pawn never appeared — intro cancelled."));
            Phase = EPhase::Finished;
            return;
        }
        TryBegin();
        return;

    case EPhase::Holding:
    case EPhase::Blending:
        break;

    default:
        return;
    }

    if (!IsIntroStillValid())
    {
        UE_LOG(LogkdLevelIntro, Log, TEXT("[LevelIntro] ABORT (death / level complete / invalid refs)."));
        EndIntro(/*bAborted*/ true);
        return;
    }

    PhaseTime += DeltaTime;

    if (Phase == EPhase::Holding)
    {
        // Slow push-in toward the goal: the shot never feels like a frozen slide.
        if (S->HoldDriftCm > KINDA_SMALL_NUMBER && IntroCamera)
        {
            const float T = FMath::Clamp(PhaseTime / FMath::Max(S->HoldSeconds, KINDA_SMALL_NUMBER), 0.f, 1.f);
            IntroCamera->SetActorLocation(HoldStartLoc + HoldDir * (S->HoldDriftCm * FMath::SmoothStep(0.f, 1.f, T)));
        }

        if (PhaseTime >= S->HoldSeconds)
        {
            BeginBlend(S->BlendSeconds);
        }
    }
    else // Blending
    {
        // Small margin so the engine's own blend lands before we destroy the camera.
        if (PhaseTime >= ActiveBlendSeconds + 0.1f)
        {
            UE_LOG(LogkdLevelIntro, Log, TEXT("[LevelIntro] FINISHED '%s'."), *LevelName.ToString());
            EndIntro(/*bAborted*/ false);
        }
    }
}

// ─── Phase transitions ────────────────────────────────────────────────────────

void UkdLevelIntroSubsystem::BeginBlend(float BlendSeconds)
{
    APlayerController* PC = CachedPC.Get();
    AkdMyPlayer* Player = CachedPlayer.Get();
    const UkdLevelIntroSettings* S = GetDefault<UkdLevelIntroSettings>();
    if (!PC || !Player || !S) { EndIntro(true); return; }

    Phase = EPhase::Blending;
    PhaseTime = 0.f;
    ActiveBlendSeconds = FMath::Max(BlendSeconds, 0.01f);

    // The engine re-evaluates the pawn's camera every frame of the blend, so
    // spring-arm lag / yaw can never cause a landing pop.
    PC->SetViewTargetWithBlend(Player, ActiveBlendSeconds, S->BlendFunction, S->BlendExp, /*bLockOutgoing*/ false);

    if (S->bPulseOnReveal)
    {
        if (UkdWorldColorDriver* Driver = Player->FindComponentByClass<UkdWorldColorDriver>())
        {
            Driver->TriggerEdgePulse(S->RevealPulseStrength, 0.6f);
        }
    }

    UE_LOG(LogkdLevelIntro, Log, TEXT("[LevelIntro] BLEND back to player over %.2fs."), ActiveBlendSeconds);
}

void UkdLevelIntroSubsystem::RequestSkip()
{
    if (Phase != EPhase::Holding) return;

    const UkdLevelIntroSettings* S = GetDefault<UkdLevelIntroSettings>();
    if (!S || !S->bAllowSkip || PhaseTime < S->SkipGraceSeconds) return;

    UE_LOG(LogkdLevelIntro, Log, TEXT("[LevelIntro] SKIP requested."));
    BeginBlend(S->SkipBlendSeconds);
}

bool UkdLevelIntroSubsystem::IsIntroStillValid() const
{
    const APlayerController* PC = CachedPC.Get();
    const AkdMyPlayer* Player = CachedPlayer.Get();
    if (!PC || !Player || !IsValid(IntroCamera)) return false;
    if (Player->IsDead()) return false;

    if (const AkdGameModeBase* GM = Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        if (GM->IsLevelComplete() || GM->IsGameOver()) return false;
    }
    return true;
}

void UkdLevelIntroSubsystem::EndIntro(bool bAborted)
{
    if (bAborted)
    {
        APlayerController* PC = CachedPC.Get();
        AkdMyPlayer* Player = CachedPlayer.Get();
        if (PC && Player)
        {
            PC->SetViewTarget(Player);   // instant — a corpse/respawn must never see the goal shot
            if (PC->PlayerCameraManager) PC->PlayerCameraManager->StopCameraFade();
        }
    }

    ReleaseIntroResources();
    Phase = EPhase::Finished;
}

void UkdLevelIntroSubsystem::ReleaseIntroResources()
{
    if (AkdPlayerController* KPC = Cast<AkdPlayerController>(CachedPC.Get()))
    {
        KPC->SetGameplayInputBlocked(false);
    }

    if (const UWorld* W = GetWorld())
    {
        if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(W->GetAuthGameMode()))
        {
            GM->SetLevelTimerSuspended(false);
        }
    }

    if (IsValid(IntroCamera))
    {
        IntroCamera->Destroy();
    }
    IntroCamera = nullptr;
}