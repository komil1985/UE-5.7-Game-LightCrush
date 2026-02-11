// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdCrushTransitionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrushTransitionComplete, bool, bIsCrushMode);


class AkdMyPlayer;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdCrushTransitionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdCrushTransitionComponent();

	UFUNCTION(BlueprintCallable, Category = "Crush | Transition")
	void StartTransition(bool bToCrushMode);

	UPROPERTY(BlueprintAssignable, Category = "Crush | Events")
	FOnCrushTransitionComplete OnTransitionComplete;

	/* -- Configuration -- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Transition")
	float TransitionDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Transition")
	FVector PlayerCrushScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Transition")
	float OrthoWidth = 1000.0f;

protected:
	virtual void BeginPlay() override;
	void HandleTransitionUpdate();
	void FinishTransition();

private:
	UPROPERTY()
	TObjectPtr<AkdMyPlayer> CachedOwner;

	FTimerHandle TransitionTimerHandle;

	float CurrentAlpha;

	UPROPERTY(EditDefaultsOnly, Category = "Crush | Transition")
	float TimerInterval = 0.016f;	// ~60 fps update rate for smooth animations

	// Cache initial values to lerp from
	FVector InitialScale;
	FVector TargetScale;

	float InitialOrthoWidth = 0.0f;
	float TargetOrthoWidth = 0.0f;
	bool bTargetCrushMode = false;
};
