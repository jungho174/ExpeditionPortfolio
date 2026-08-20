// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleEncounterTrigger.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API ABattleEncounterTrigger : public AActor
{
	GENERATED_BODY()

public:
	ABattleEncounterTrigger();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleEncounterBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> EncounterVolume;

	bool bEncounterTriggered = false;
};
