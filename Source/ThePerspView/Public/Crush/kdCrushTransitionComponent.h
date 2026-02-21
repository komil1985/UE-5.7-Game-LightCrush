// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdCrushTransitionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrushTransitionComplete, bool, bIsCrushMode);

class UCurveFloat;
class AkdMyPlayer;
class UTimelineComponent;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdCrushTransitionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdCrushTransitionComponent();

	UFUNCTION(BlueprintCallable, Category = "Crush | System")
	void StartTransition(bool bToCrushMode);

	UPROPERTY(BlueprintAssignable, Category = "Crush | System")
	FOnCrushTransitionComplete OnTransitionComplete;

	/* -- Configuration -- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Settings | Transition", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float TransitionDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Settings | Transition")
	FVector PlayerCrushScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Settings | Transition")
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

	UPROPERTY()
	TObjectPtr<UTimelineComponent> CrushTimeline;		// Timeline for smooth transition 

	UFUNCTION()
	void HandleTimelineUpdate(float Value);

	UFUNCTION()
	void HandleTimelineFinished();

	UPROPERTY(EditDefaultsOnly, Category = "Crush Settings | Transition")
	float TimerInterval = 0.016f;	// ~60 fps update rate for smooth animations

	UPROPERTY(EditDefaultsOnly, Category = "Crush Settings | Transition")
	TObjectPtr<UCurveFloat> TransitionCurve;

	UPROPERTY(EditAnywhere, Category = "Crush Settings|Camera")
	float TargetOrthoWidth = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Crush Settings|Visuals")
	FVector CrushScaleTarget = FVector(1.0f, 1.0f, 1.0f);

	// Cache initial values to lerp from
	FVector InitialScale;
	FVector TargetScale;
	float InitialOrthoWidth;
	float TargetOrthoWidthInternal;

	bool bTargetCrushMode = false;
	float CurrentElapsedTime;

};
