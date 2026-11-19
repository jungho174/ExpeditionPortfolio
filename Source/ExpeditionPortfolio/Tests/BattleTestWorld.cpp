// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/BattleTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Combat/EnemyAttackPatternData.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

class FBattleCoordinatorTestAccess
{
public:
	static void ConfigureTestWorld(UBattleCoordinator& Battle, float TurnDelay)
	{
		Battle.TestTurnTransitionDelay = TurnDelay;
		Battle.bContentFreeTest = true;
	}
};

namespace Expedition::BattleTest
{
	FScopedBattleTestWorld::FScopedBattleTestWorld(float TurnDelay)
	{
		const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("BattleCoordinatorTestWorld"));
		World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
		if (World)
		{
			AdvanceBattleTime(World, 0.0f);
			FBattleCoordinatorTestAccess::ConfigureTestWorld(*GetCoordinator(), TurnDelay);
		}
	}

	FScopedBattleTestWorld::~FScopedBattleTestWorld()
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
	}

	UBattleCoordinator* FScopedBattleTestWorld::GetCoordinator() const
	{
		return World ? World->GetSubsystem<UBattleCoordinator>() : nullptr;
	}

	UCombatantComponent* AddCombatantComponent(AActor* Owner)
	{
		UCombatantComponent* Combatant = NewObject<UCombatantComponent>(Owner);
		Owner->AddInstanceComponent(Combatant);
		Combatant->RegisterComponent();
		Combatant->EnemyAttackPatterns.Add(NewObject<UEnemyAttackPatternData>(Owner));
		return Combatant;
	}

	void AdvanceBattleTime(UWorld* World, const float DeltaSeconds)
	{
		++GFrameCounter;
		World->TimeSeconds += DeltaSeconds;
		World->GetTimerManager().Tick(DeltaSeconds);
	}

	void AdvanceToDefenseWindow(UWorld* World)
	{
		AdvanceBattleTime(World, 1.01f);
	}

	void ExpireDefenseWindow(UWorld* World)
	{
		AdvanceBattleTime(World, 0.61f);
	}

	void CompleteNoInputEnemyTurn(UWorld* World)
	{
		AdvanceToDefenseWindow(World);
		ExpireDefenseWindow(World);
	}
}

#endif
