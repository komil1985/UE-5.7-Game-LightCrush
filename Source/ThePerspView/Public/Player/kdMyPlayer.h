// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "kdMyPlayer.generated.h"


class UCameraComponent;
class USpringArmComponent;
class AkdFloorBase;
UCLASS()
class THEPERSPVIEW_API AkdMyPlayer : public ACharacter
{
	GENERATED_BODY()

public:
	AkdMyPlayer();
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UFUNCTION(BlueprintCallable, Category = "Mechanic")
	void ToggleCrushMode();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	AkdFloorBase* FindFloorActor(UWorld* World);

	UPROPERTY()
	TObjectPtr<AkdFloorBase>FloorActor = nullptr;

private:
	bool bIsCrushMode;
};
