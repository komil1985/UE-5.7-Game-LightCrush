// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "kdShadowEnemy.generated.h"

class UGameplayEffect;
UCLASS()
class THEPERSPVIEW_API AkdShadowEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	AkdShadowEnemy();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	// ── Patrol ───────────────────────────────────────────────────────────────

	/** Half the total patrol distance (cm). Enemy patrols spawn.Y ± this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	float PatrolHalfDistance = 500.f;

	/** Movement speed along the patrol axis (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	float PatrolSpeed = 150.f;

	// ── Combat ───────────────────────────────────────────────────────────────

	/**
	 * GameplayEffect applied to the player on contact.
	 * Use an Instant effect that subtracts ShadowStamina (e.g. -50).
	 * Only fires when the player is in State.CrushMode + State.InShadow.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TSubclassOf<UGameplayEffect> ContactEffectClass;

	/** Cooldown between successive hits so one overlap doesn't drain instantly. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.1"))
	float ContactCooldown = 1.f;

private:
	// Patrol state
	FVector PatrolPointA;
	FVector PatrolPointB;
	bool    bMovingToB = true;

	// Contact cooldown
	bool          bCanDamage = true;
	FTimerHandle  DamageCooldownHandle;

	UFUNCTION()
	void OnCapsuleBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32                OtherBodyIndex,
		bool                 bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void ResetDamageCooldown();
};


// AkdShadowEnemy — a simple patrol character that lives on the shadow plane.
//
// BEHAVIOUR:
//   • Patrols left/right along the Y axis between ±PatrolHalfDistance from
//     its spawn location. No AI controller needed.
//   • Only deals damage when the player overlaps AND the player has both
//     State.CrushMode and State.InShadow — harmless in 3D mode.
//   • On contact: applies ContactEffectClass to the player's ASC (set this to
//     an Instant GameplayEffect that subtracts stamina, e.g. -50).
//
// SETUP (editor):
//   1. Create a Blueprint subclass of AkdShadowEnemy.
//   2. Assign a skeletal/static mesh.
//   3. Set ContactEffectClass to your chosen instant stamina-drain effect.
//   4. Tune PatrolHalfDistance and PatrolSpeed per level need.
//
// VISIBILITY:
//   The enemy is always rendered. For early prototype this is intentional —
//   add opacity material logic in the Blueprint subclass when needed.