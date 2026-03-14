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

protected:
	 UPROPERTY(meta = (BindWidget))
	 TObjectPtr<UProgressBar> StaminaBar;

	 void OnStaminaChanged(const FOnAttributeChangeData& Data);
	 void OnMaxStaminaChanged(const FOnAttributeChangeData& Data);

private:

	float CurrentStamina;
	float MaxStamina;

	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
};
