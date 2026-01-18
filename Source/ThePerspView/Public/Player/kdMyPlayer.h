// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "kdMyPlayer.generated.h"

UCLASS()
class THEPERSPVIEW_API AkdMyPlayer : public ACharacter
{
	GENERATED_BODY()

public:
	AkdMyPlayer();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

};
