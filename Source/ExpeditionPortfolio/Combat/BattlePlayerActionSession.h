#pragma once

#include "CoreMinimal.h"
#include "Combat/AttackTiming.h"
#include "Combat/BattlePlayerCommand.h"
#include "Combat/CharacterActionData.h"
#include "TimerManager.h"
#include "BattlePlayerActionSession.generated.h"

class UCombatItemData;
class UItemPresentationComponent;
class UMagicPresentationComponent;
class UPlayerUtilityPresentationComponent;

// 확정된 명령이 끝날 때까지 대상·효과·연출을 함께 보관한다.
USTRUCT()
struct FBattlePlayerActionSession
{
	GENERATED_BODY()

	int64 Id = 0;
	EBattlePlayerCommand Command = EBattlePlayerCommand::BasicAttack;
	EPlayerActionPhase Phase = EPlayerActionPhase::None;
	UPROPERTY(Transient) TObjectPtr<UCharacterActionData> Action;
	UPROPERTY(Transient) TObjectPtr<UCombatItemData> Item;
	TArray<TWeakObjectPtr<AActor>> Targets;
	ECharacterActionTargetScope TargetScope = ECharacterActionTargetScope::SingleEnemy;
	float EffectAmount = 0.0f;
	bool bHitApplied = false;
	int64 QTESessionId = 0;
	FTimerHandle TimeoutTimer;
	TWeakObjectPtr<UActorComponent> ActionPresentation;
	TWeakObjectPtr<UItemPresentationComponent> ItemPresentation;
#if WITH_DEV_AUTOMATION_TESTS
	TWeakObjectPtr<UMagicPresentationComponent> MagicPresentation;
	TWeakObjectPtr<UPlayerUtilityPresentationComponent> UtilityPresentation;
#endif

	bool Matches(int64 ReportedId) const { return ReportedId != 0 && ReportedId == Id; }
	bool CanApplyHit() const { return !bHitApplied && Phase != EPlayerActionPhase::Preparing; }
	void Stop(UWorld* World);
};
