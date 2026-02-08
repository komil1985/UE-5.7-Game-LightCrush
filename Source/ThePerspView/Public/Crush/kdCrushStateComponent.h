// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdCrushStateComponent.generated.h"

class ACharacter;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdCrushStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdCrushStateComponent();

	UFUNCTION(BlueprintCallable, Category = "LightCrush")
	void EnterCrushState();									

	UFUNCTION(BlueprintCallable, Category = "LightCrush")
	void ExitCrushState();

	UFUNCTION(BlueprintCallable, Category = "LightCrush")
	bool IsInCrushState() const { return bIsCrushed; }

protected:
	virtual void BeginPlay() override;

private:
	bool bIsCrushed = false;

	UPROPERTY()
	TObjectPtr<ACharacter> PlayerCharacter;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CrushPPInstance;

	void ApplyCrushEffect();
	void RemoveCrushEffect();

	void ConfigureMovementForCrush(bool bEnable);
};
