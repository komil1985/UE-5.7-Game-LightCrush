// Copyright ASKD Games


#include "Components/kdPortalTeleportComponent.h"
#include "Player/kdMyPlayer.h"
#include "World/kdShadowPortal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

UkdPortalTeleportComponent::UkdPortalTeleportComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // pure timer-driven, like the death fade
}

void UkdPortalTeleportComponent::BeginPlay()
{
	Super::BeginPlay();
	CachedPlayer = Cast<AkdMyPlayer>(GetOwner());

#if !UE_BUILD_SHIPPING
	if (!CachedPlayer)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UkdPortalTeleportComponent: owner is not AkdMyPlayer — disabled."));
	}
#endif
}

void UkdPortalTeleportComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	// Never leave the screen held on indigo if we're torn down mid-teleport.
	if (bTeleporting)
	{
		CancelTeleport();
	}
	Super::EndPlay(Reason);
}

// ── Public entry ────────────────────────────────────────────────────────────────

void UkdPortalTeleportComponent::BeginTeleport(const FVector& ExitLocation, AkdShadowPortal* DestinationPortal)
{
	if (bTeleporting || !CachedPlayer)
	{
		return; // re-entrancy guard: one teleport at a time
	}

	bTeleporting = true;
	PendingExitLocation = ExitLocation;
	PendingDest = DestinationPortal;

	SetInputEnabled(false);

	// Swallow: fade to indigo and HOLD it opaque until we move under cover.
	if (APlayerCameraManager* Cam = GetCameraManager())
	{
		Cam->StartCameraFade(
			0.f,               // from (clear)
			1.f,               // to   (opaque)
			FadeOutSeconds,
			FadeColor,
			false,             // bShouldFadeAudio
			true);             // bHoldWhenFinished — stays covered for the move
	}

	// Advance to the covered move once the screen is fully opaque.
	GetWorld()->GetTimerManager().SetTimer(
		CoveredTimer, this, &UkdPortalTeleportComponent::HandleCovered,
		FMath::Max(FadeOutSeconds, 0.001f), false);
}

// ── Step 1: screen fully covered — do the move nobody can see ────────────────────

void UkdPortalTeleportComponent::HandleCovered()
{
	if (!CachedPlayer)
	{
		FinishTeleport();
		return;
	}

	// Glue the camera so lag can't fly the view across the level on reveal.
	if (bFreezeCameraLagDuringMove)
	{
		SetSpringArmLagEnabled(false);
	}

	// Move the capsule. TeleportPhysics avoids sweeping the player into walls.
	CachedPlayer->SetActorLocation(
		PendingExitLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Keep in-plane momentum; drop the collapse-axis component so the exit is clean.
	if (UCharacterMovementComponent* MC = CachedPlayer->GetCharacterMovement())
	{
		FVector V = MC->Velocity;
		if (bZeroCollapseAxisVelocity)
		{
			V.X = 0.f;
		}
		MC->Velocity = V;
	}

	// The destination black hole exhales exactly as the player arrives.
	if (AkdShadowPortal* Dest = PendingDest.Get())
	{
		Dest->OnPlayerArrived();
	}

	// Optional punch for impact on the reveal.
	if (ArrivalShakeClass)
	{
		if (APlayerCameraManager* Cam = GetCameraManager())
		{
			Cam->StartCameraShake(ArrivalShakeClass);
		}
	}

	// Hold the black beat, then reveal.
	GetWorld()->GetTimerManager().SetTimer(
		RevealTimer, this, &UkdPortalTeleportComponent::HandleReveal,
		FMath::Max(HoldSeconds, 0.001f), false);
}

// ── Step 2: fade back in ─────────────────────────────────────────────────────────

void UkdPortalTeleportComponent::HandleReveal()
{
	if (APlayerCameraManager* Cam = GetCameraManager())
	{
		Cam->StartCameraFade(
			1.f, 0.f, FadeInSeconds, FadeColor, false, false /*don't hold*/);
	}

	GetWorld()->GetTimerManager().SetTimer(
		FinishTimer, this, &UkdPortalTeleportComponent::FinishTeleport,
		FMath::Max(FadeInSeconds, 0.001f), false);
}

// ── Step 3: hand control back ────────────────────────────────────────────────────

void UkdPortalTeleportComponent::FinishTeleport()
{
	if (bFreezeCameraLagDuringMove)
	{
		SetSpringArmLagEnabled(true); // restore whatever lag settings we captured
	}

	SetInputEnabled(true);

	PendingDest = nullptr;
	bTeleporting = false;
}

// ── Cancel path ──────────────────────────────────────────────────────────────────

void UkdPortalTeleportComponent::CancelTeleport()
{
	FTimerManager& TM = GetWorld()->GetTimerManager();
	TM.ClearTimer(CoveredTimer);
	TM.ClearTimer(RevealTimer);
	TM.ClearTimer(FinishTimer);

	if (bFreezeCameraLagDuringMove)
	{
		SetSpringArmLagEnabled(true);
	}

	// Clear any held fade instantly so we don't strand a black screen.
	if (APlayerCameraManager* Cam = GetCameraManager())
	{
		Cam->StartCameraFade(0.f, 0.f, 0.f, FadeColor, false, false);
	}

	SetInputEnabled(true);
	PendingDest = nullptr;
	bTeleporting = false;
}

// ── Helpers ──────────────────────────────────────────────────────────────────────

void UkdPortalTeleportComponent::SetInputEnabled(bool bEnabled)
{
	if (!CachedPlayer) return;
	APlayerController* PC = GetPC();
	if (!PC) return;

	// Same mechanism the death fade uses: block movement/action bindings while
	// leaving UI input alone, plus the belt-and-braces move-input ignore.
	if (bEnabled)
	{
		CachedPlayer->EnableInput(PC);
		PC->SetIgnoreMoveInput(false);
	}
	else
	{
		CachedPlayer->DisableInput(PC);
		PC->SetIgnoreMoveInput(true);
	}
}

void UkdPortalTeleportComponent::SetSpringArmLagEnabled(bool bEnabled)
{
	if (!CachedPlayer || !CachedPlayer->SpringArm) return;
	USpringArmComponent* Arm = CachedPlayer->SpringArm;

	if (!bEnabled)
	{
		// Capture current state before we force it off.
		bSavedLocationLag = Arm->bEnableCameraLag;
		bSavedRotationLag = Arm->bEnableCameraRotationLag;
		Arm->bEnableCameraLag = false;
		Arm->bEnableCameraRotationLag = false;
	}
	else
	{
		Arm->bEnableCameraLag = bSavedLocationLag;
		Arm->bEnableCameraRotationLag = bSavedRotationLag;
	}
}

APlayerController* UkdPortalTeleportComponent::GetPC() const
{
	return UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

APlayerCameraManager* UkdPortalTeleportComponent::GetCameraManager() const
{
	APlayerController* PC = GetPC();
	return PC ? PC->PlayerCameraManager : nullptr;
}
