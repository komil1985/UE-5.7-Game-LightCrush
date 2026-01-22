// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "kdPlayerController.generated.h"

/**
 * 
 */
struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
UCLASS()
class THEPERSPVIEW_API AkdPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CrushAction;

private:
	void Move(const FInputActionValue& Value);
	void StartJump();
	void StopJump();
	void CrushMode();

	void EnhancedSubSystem();
};
