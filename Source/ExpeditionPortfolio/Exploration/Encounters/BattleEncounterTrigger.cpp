// Copyright Epic Games, Inc. All Rights Reserved.

#include "Exploration/Encounters/BattleEncounterTrigger.h"

#include "Combat/BattleCoordinator.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

ABattleEncounterTrigger::ABattleEncounterTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	EncounterVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("EncounterVolume"));
	SetRootComponent(EncounterVolume);
	EncounterVolume->InitBoxExtent(FVector(100.0, 100.0, 100.0));
	EncounterVolume->SetCollisionProfileName(TEXT("Trigger"));
	EncounterVolume->SetGenerateOverlapEvents(true);
}

void ABattleEncounterTrigger::BeginPlay()
{
	Super::BeginPlay();

	EncounterVolume->OnComponentBeginOverlap.AddDynamic(
		this,
		&ABattleEncounterTrigger::HandleEncounterBeginOverlap);
}

void ABattleEncounterTrigger::HandleEncounterBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (bEncounterTriggered)
	{
		return;
	}

	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (OverlappingPawn == nullptr || PlayerController == nullptr || PlayerController->GetPawn() != OverlappingPawn)
	{
		return;
	}

	UBattleCoordinator* BattleCoordinator = GetWorld()->GetSubsystem<UBattleCoordinator>();
	if (BattleCoordinator == nullptr || !BattleCoordinator->RequestBattleStart(this, OverlappingPawn))
	{
		return;
	}

	bEncounterTriggered = true;
	EncounterVolume->SetGenerateOverlapEvents(false);
	EncounterVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
