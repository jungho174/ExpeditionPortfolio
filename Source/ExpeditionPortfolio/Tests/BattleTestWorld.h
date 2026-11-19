// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

class AActor;
class UBattleCoordinator;
class UCombatantComponent;
class UWorld;

namespace Expedition::BattleTest
{
	// 임시 월드에서만 에셋 없는 전투를 허용하고, 검사가 끝나면 월드를 해제한다.
	class FScopedBattleTestWorld
	{
	public:
		explicit FScopedBattleTestWorld(float TurnDelay = 0.0f);
		~FScopedBattleTestWorld();

		FScopedBattleTestWorld(const FScopedBattleTestWorld&) = delete;
		FScopedBattleTestWorld& operator=(const FScopedBattleTestWorld&) = delete;

		UBattleCoordinator* GetCoordinator() const;
		UWorld* GetWorld() const { return World; }

	private:
		UWorld* World = nullptr;
	};

	// 기본 공격 패턴 한 개를 가진 전투 참가자를 등록한다.
	UCombatantComponent* AddCombatantComponent(AActor* Owner);

	// 한 프레임씩 전진시켜 월드 시간과 전투 타이머를 함께 갱신한다.
	void AdvanceBattleTime(UWorld* World, float DeltaSeconds);
	void AdvanceToDefenseWindow(UWorld* World);
	void ExpireDefenseWindow(UWorld* World);
	void CompleteNoInputEnemyTurn(UWorld* World);
}

#endif
