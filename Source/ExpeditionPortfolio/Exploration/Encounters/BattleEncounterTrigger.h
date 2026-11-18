// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleEncounterTrigger.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class APawn;

UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API ABattleEncounterTrigger : public AActor
{
	GENERATED_BODY()

public:
	ABattleEncounterTrigger();
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Encounter|Persistence") FGuid EncounterId;
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Encounter|Persistence") void EnsureEncounterId();
	UFUNCTION(BlueprintPure, Category="Encounter|Persistence") FName GetEncounterMapId() const;
	UFUNCTION(BlueprintPure, Category="Encounter|Persistence") bool IsEncounterCleared() const;
	void CompleteEncounter();
	bool FindRetreatLocation(APawn* Player, FVector& OutLocation) const;
	void RearmAfterRetreat();
	UFUNCTION(BlueprintPure,Category="Encounter") virtual TArray<AActor*> GetEncounterEnemies() const;
	UPROPERTY(EditInstanceOnly,BlueprintReadOnly,Category="Encounter|Party") TArray<TObjectPtr<APawn>> AdditionalAllies;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Retreat", meta=(ClampMin="20", Units="cm")) float RetreatDistance=100;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Encounter|Retreat") TObjectPtr<AActor> RetreatPoint;

protected:
	virtual void BeginPlay() override;
	virtual void PostActorCreated() override;
#if WITH_EDITOR
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditImport() override;
#endif

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

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> EnemyActor;

	bool bEncounterTriggered = false;
	bool bEncounterCleared = false;
	FVector EntryBackDirection = FVector::BackwardVector;
};
