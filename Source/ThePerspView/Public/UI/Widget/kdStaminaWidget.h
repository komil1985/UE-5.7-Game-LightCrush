// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "kdStaminaWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
struct FOnAttributeChangeData;
/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API UkdStaminaWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitializeWithAbilitySystemComponent(UAbilitySystemComponent* ASC);

	UFUNCTION()
	void SetStaminaBarVisibility(bool bIsVisible);

protected:
	 UPROPERTY(meta = (BindWidget))
	 TObjectPtr<UProgressBar> StaminaBar;

	 void OnStaminaChanged(const FOnAttributeChangeData& Data);
	 void OnMaxStaminaChanged(const FOnAttributeChangeData& Data);

private:

	void UpdateVisibility();

	float CurrentStamina;
	float MaxStamina;

	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
};
