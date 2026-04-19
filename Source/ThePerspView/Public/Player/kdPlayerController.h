// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/kdGameplayTags.h"
#include "kdPlayerController.generated.h"

/**
 * 
 */
struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
class AkdMyPlayer;
class UAbilitySystemComponent;
UCLASS()
class THEPERSPVIEW_API AkdPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/*	--	Input Actions --	*/	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveInShadowAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CrushAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> PauseAction;
	/*-----------------------------------------------------------------*/

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartJump();
	void StopJump();
	void Interact();
	void ShadowDash();
	void TogglePause();

	void EnhancedSubSystem();
	void CrushToggleRequest();
	//void HandleShadowMovement(const FInputActionValue& Value);
	void PrintTags();
	
	TObjectPtr<AkdMyPlayer> MyPlayerCache;
	TObjectPtr<UAbilitySystemComponent> MyASC;

	const FkdGameplayTags& StateTags = FkdGameplayTags::Get();
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DebugPrintTagsAction;

};
