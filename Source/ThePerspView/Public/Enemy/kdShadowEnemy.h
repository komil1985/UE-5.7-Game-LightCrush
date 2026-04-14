// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ActiveGameplayEffectHandle.h"
#include "kdShadowEnemy.generated.h"

class UGameplayEffect;
class USphereComponent;
class AkdMyPlayer;
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

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.1"))
	float DamageTickRate = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TObjectPtr<USphereComponent> DamageSphere;

private:
	TWeakObjectPtr<AkdMyPlayer> OverlappingPlayer;

	// Patrol state
	FVector PatrolPointA;
	FVector PatrolPointB;
	bool bMovingToB = true;

	UFUNCTION()
	void TryApplyContactDamage();

	FTimerHandle DamageTickHandle;

	FActiveGameplayEffectHandle ContactEffectHandle;

	UFUNCTION()
	void OnDamageSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnDamageSphereEndOverlap(
		UPrimitiveComponent* OverlappedComp, 
		AActor* OtherActor,	
		UPrimitiveComponent* OtherComp, 
		int32 OtherBodyIndex);
};


// AkdShadowEnemy — patrols the shadow plane (Y axis) and damages the player on contact.
//
// KEY DESIGN DECISIONS vs the old version:
//
// 1. PATROL via SetActorLocation (not AddMovementInput)
//    AddMovementInput + Walking mode requires a nav mesh and a walkable floor
//    directly under the capsule. Shadow plane enemies are placed on walls or
//    mid-air surfaces where there is no such floor. Direct position update is
//    more predictable and has zero dependencies on nav mesh or AI controllers.
//
// 2. GravityScale = 0, MovementMode = MOVE_Flying
//    Prevents the enemy from falling off the shadow surface it is placed on.
//
// 3. SEPARATE DamageSphere for overlap detection (not the capsule)
//    ACharacter's capsule uses the "Pawn" collision profile which sets ECC_Pawn
//    to BLOCK. OnComponentBeginOverlap only fires for OVERLAP responses, never
//    for BLOCK. The capsule keeps blocking (so the enemy is a solid obstacle),
//    and the slightly larger DamageSphere is set to OverlapAllDynamic so the
//    contact callback fires correctly.
//
// SETUP:
//   1. Create a Blueprint subclass. Assign a mesh.
//   2. Set ContactEffectClass to an Instant GE that subtracts ShadowStamina.
//   3. Place on the shadow surface (e.g. a wall the player patrols along).
//   4. Tune PatrolHalfDistance and PatrolSpeed.