// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "kdAttributeSet.generated.h"

// Macro to define getter/setter/initter helpers automatically
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API UkdAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
    UkdAttributeSet();

    // Shadow Stamina while moving in shadows
    UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData ShadowStamina;
    ATTRIBUTE_ACCESSORS(UkdAttributeSet, ShadowStamina);

    // Max Shadow Stamina
    UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData MaxShadowStamina;
	ATTRIBUTE_ACCESSORS(UkdAttributeSet, MaxShadowStamina);


};
