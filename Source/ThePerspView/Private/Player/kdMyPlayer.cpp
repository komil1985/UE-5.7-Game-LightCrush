// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/kdMyPlayer.h"

AkdMyPlayer::AkdMyPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

}

void AkdMyPlayer::BeginPlay()
{
	Super::BeginPlay();
	
}

void AkdMyPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AkdMyPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

