#pragma once

#include "CoreMinimal.h"
#include "Combat/BattleDefense.h"
#include "Combat/BattleFacing.h"
#include "Combat/BattleResult.h"
#include "Combat/CombatantComponent.h"
#include "BattleEncounterRecord.generated.h"

struct FBattleResources
{
	TMap<FName, int32> ItemStocks;
	TMap<TWeakObjectPtr<AActor>, TMap<FName, int32>> ActionUses;
	int32 RemainingPotions = 0;
	bool bPowerStrikeUsed = false;
};

struct FBattleParticipantEntry
{
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UCombatantComponent> Combatant;
	FTransform Transform;
	float Health = 0.0f;
	bool bAlly = false;
	FDelegateHandle HealthHandle;
	FBattleFacingRestore FacingRestore;
};

struct FBattleEncounterEntry
{
	TArray<FBattleParticipantEntry> Participants;
	FBattleResources Resources;

	void RestoreParticipants() const;
};

// 진입 시점의 복원 자료와 전투 중 누적되는 결과를 한 조우의 수명에 묶는다.
USTRUCT()
struct FBattleEncounterRecord
{
	GENERATED_BODY()

	FBattleEncounterEntry Entry;
	UPROPERTY(Transient) FBattleResult Result;
	bool bReady = false;

	void BindParticipant(AActor* Actor, bool bAlly, const FOnCombatantHealthChanged::FDelegate& OnChange);
	void ReleaseParticipants();
	void RecordHealthChange(bool bAlly, float Before, float After);
	void RecordDefense(EBattleDefenseResult Defense);
	void CaptureOutcome(bool bVictory, const AActor* ActingAlly);
};
