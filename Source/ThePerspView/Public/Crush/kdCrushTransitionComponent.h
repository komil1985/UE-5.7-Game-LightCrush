// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdCrushTransitionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrushTransitionComplete, bool, bIsCrushMode);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdCrushTransitionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdCrushTransitionComponent();

	UFUNCTION(BlueprintCallable, Category = "Crush Transition")
	void StartTransition(bool bToCrushMode);

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnCrushTransitionComplete OnTransitionComplete;

	/* -- Configuration -- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Transition")
	float TransitionDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Transition")
	FVector PlayerCrushScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Transition")
	float OrthoWidth = 1000.0f;

protected:
	void HandleTransitionUpdate();
	void FinishTransition();

private:
	FTimerHandle TransitionTimerHandle;

	bool bTargetCrushMode;
	float CurrentAlpha;
	float TimerInterval = 0.016f;	// ~60 fps update rate for smooth animations

	// Cache initial values to lerp from
	FVector InitialScale;
	FVector TargetScale;
};
