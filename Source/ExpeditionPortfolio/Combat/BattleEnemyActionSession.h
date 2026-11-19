#pragma once

#include "CoreMinimal.h"
#include "Combat/BattleDefense.h"
#include "Combat/EnemyAttackPatternData.h"
#include "TimerManager.h"
#include "BattleEnemyActionSession.generated.h"

class UCharacterActionData;
class UEnemyAttackPresentationComponent;

// 적의 한 공격에 속한 다단 타격과 반격은 같은 행동 ID를 공유한다.
USTRUCT()
struct FBattleEnemyActionSession
{
	GENERATED_BODY()

	UPROPERTY(Transient) FEnemyAttackPatternSnapshot Pattern;
	bool bHasPattern = false;
	int64 Id = 0;
	int32 HitIndex = 0;
	bool bAllHitsParried = true;
	bool bAttackStarted = false;
	float AttackStartedAt = 0.0f;
	EBattleDefenseResult BufferedDefense = EBattleDefenseResult::None;
	bool bHitResolved = false;
	bool bReceivedHitNotify = false;
	bool bWaitingForAnimation = false;
	TWeakObjectPtr<UEnemyAttackPresentationComponent> Presentation;
	FTimerHandle TelegraphTimer;
	FTimerHandle HitTimer;
	FTimerHandle WindowOpenTimer;
	FTimerHandle TimeoutTimer;

	UPROPERTY(Transient) TObjectPtr<UCharacterActionData> CounterAction;
	TWeakObjectPtr<UActorComponent> CounterPresentation;
	bool bCounterActive = false;
	bool bCounterHitApplied = false;
	float CounterDamage = 15.0f;
	FTimerHandle CounterTimer;

	bool Matches(int64 ReportedId) const { return ReportedId != 0 && ReportedId == Id; }
	bool MatchesHit(int64 ReportedId, int32 Index) const { return Matches(ReportedId) && HitIndex == Index; }
	void Begin(int64 NewId);
	void PrepareCurrentHit();
	void ResetHit();
	void ClearTimers(FTimerManager& Timers);
};
