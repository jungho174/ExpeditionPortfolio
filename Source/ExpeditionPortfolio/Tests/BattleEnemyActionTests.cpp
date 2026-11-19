// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Tests/BattleTestWorld.h"

using Expedition::BattleTest::FScopedBattleTestWorld;
using Expedition::BattleTest::AddCombatantComponent;
using Expedition::BattleTest::AdvanceBattleTime;
using Expedition::BattleTest::AdvanceToDefenseWindow;
using Expedition::BattleTest::ExpireDefenseWindow;
using Expedition::BattleTest::CompleteNoInputEnemyTurn;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyComboLifecycleTest,
	"ExpeditionPortfolio.Combat.EnemyCombo.Lifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FEnemyComboLifecycleTest::RunTest(const FString& Parameters)
{
	for (int32 Case=0;Case<7;++Case)
	{
		FScopedBattleTestWorld TestWorld;
		UWorld* World = TestWorld.GetWorld();
		UBattleCoordinator* Battle = TestWorld.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>();
		auto AddStats=[](AActor* Owner) { auto* Stats=NewObject<UCombatantComponent>(Owner); Owner->AddInstanceComponent(Stats); Stats->RegisterComponent(); return Stats; };
		auto* PlayerHP=AddStats(Player); auto* EnemyHP=AddStats(Enemy);
		auto* Pattern=NewObject<UEnemyAttackPatternData>(Enemy); Pattern->PatternId=TEXT("Triple");
		for (float Time:{.6f,1.3f,2.f}) { FEnemyAttackHit Hit; Hit.Damage=10; Hit.FallbackHitTime=Time; Pattern->Hits.Add(Hit); }
		EnemyHP->EnemyAttackPatterns.Add(Pattern);
		Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy); Battle->RequestPlayerBasicAttack(); AdvanceBattleTime(World, 1.01f);
		const int64 Id=Battle->ActiveEnemyActionId;
		TestEqual(TEXT("Three distinct hits copied"),Battle->GetEnemyHitCount(),3);
		TestFalse(TEXT("Future indexed marker cannot hit current window"),Battle->NotifyEnemyActionHit(Id,1));
		if (Case==6) { Pattern->Hits[1].Damage=99; Pattern->Hits[1].FallbackHitTime=20; }
		int32 Parries=0;
		Battle->OnDefenseResultChanged().AddLambda([&](EBattleDefenseResult Result) { if (Result==EBattleDefenseResult::ParrySuccess) { ++Parries; } });
		for (int32 Index=0;Index<3;++Index)
		{
			AdvanceBattleTime(World, Index==0?.49f:.58f);
			const bool bMiss=Case==4 || (Case==1 && Index==1);
			if (!bMiss)
			{
				const bool bDodge=Case==2 && Index==1;
				TestTrue(TEXT("New hit accepts a fresh defense"),bDodge?Battle->RequestDodge():Battle->RequestParry());
				TestFalse(TEXT("Repeat input in same hit rejected"),Battle->RequestParry());
			}
			AdvanceBattleTime(World, .12f);
			TestFalse(TEXT("Previous indexed marker rejected"),Battle->NotifyEnemyActionHit(Id,Index));
			if (Case==3 && Index==0)
			{
				TestTrue(TEXT("Interruption accepted"),Battle->NotifyEnemyActionFinished(Id,true)); AdvanceBattleTime(World, 5);
				TestEqual(TEXT("Remaining interrupted hits cancelled"),PlayerHP->GetCurrentHealth(),100.f); break;
			}
			if (Case==5 && Index==0) { PlayerHP->ApplyDamage(95); }
			if (Case==5 && Index==1) { AdvanceBattleTime(World, 3); break; }
		}
		if (Case==0 || Case==6) { TestEqual(TEXT("Only complete parry earns one counter"),EnemyHP->GetCurrentHealth(),60.f); TestEqual(TEXT("Every parry emits feedback"),Parries,3); }
		if (Case==1) { TestEqual(TEXT("Middle miss damages once using copied data"),PlayerHP->GetCurrentHealth(),90.f); TestEqual(TEXT("Partial parry earns no counter"),EnemyHP->GetCurrentHealth(),75.f); }
		if (Case==2) { TestEqual(TEXT("Mixed dodge avoids damage"),PlayerHP->GetCurrentHealth(),100.f); TestEqual(TEXT("Mixed dodge earns no counter"),EnemyHP->GetCurrentHealth(),75.f); }
		if (Case==4) { TestEqual(TEXT("Each unguarded hit damages once"),PlayerHP->GetCurrentHealth(),70.f); }
		if (Case!=5) { TestEqual(TEXT("Combo ends in command turn"),Battle->GetCurrentState(),EBattleState::PlayerCommand); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorPatternExecutionTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.PatternExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorPatternExecutionTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();
	AActor* Encounter = World->SpawnActor<AActor>();
	APawn* Player = World->SpawnActor<APawn>();
	AActor* Enemy = World->SpawnActor<AActor>();
	UCombatantComponent* PlayerCombatant = AddCombatantComponent(Player);
	UCombatantComponent* EnemyCombatant = AddCombatantComponent(Enemy);
	UEnemyAttackPatternData* Pattern = EnemyCombatant->EnemyAttackPatterns[0];
	Pattern->PatternId = TEXT("SlowAttack");
	Pattern->Damage = 37.0f;
	Pattern->TelegraphDuration = 2.0f;
	Pattern->DefenseWindowDuration = 0.9f;
	Pattern->ParrySuccessDuration = 0.4f;
	bool bSelectedBeforeEvent = false;
	const FDelegateHandle Handle = Coordinator->OnBattleStateChanged().AddLambda(
		[&](EBattleState Previous, EBattleState Next)
		{
			if (Next == EBattleState::EnemyTelegraph)
			{
				bSelectedBeforeEvent = Coordinator->HasActiveEnemyPattern()
					&& Coordinator->GetActiveEnemyPattern().PatternId == TEXT("SlowAttack");
			}
		});
	TestTrue(TEXT("Start battle"), Coordinator->RequestBattleStart(Encounter, Player, Enemy));
	TestTrue(TEXT("Attack"), Coordinator->RequestPlayerBasicAttack());
	TestTrue(TEXT("Pattern visible before telegraph notification"), bSelectedBeforeEvent);
	// Mutation must affect only the next selection, not the current attack.
	Pattern->Damage = 5.0f;
	Pattern->TelegraphDuration = 0.1f;
	Pattern->DefenseWindowDuration = 0.2f;
	Pattern->ParrySuccessDuration = 0.1f;
	AdvanceBattleTime(World, 1.1f);
	TestEqual(TEXT("Snapshot keeps longer telegraph"), Coordinator->GetCurrentState(), EBattleState::EnemyTelegraph);
	AdvanceBattleTime(World, 0.91f);
	TestEqual(TEXT("Configured telegraph opens defense"), Coordinator->GetCurrentState(), EBattleState::DefenseWindow);
	AdvanceBattleTime(World, 0.5f);
	TestTrue(TEXT("Configured parry window exceeds former 0.2s"), Coordinator->RequestParry());
	AdvanceBattleTime(World, 0.41f);
	TestEqual(TEXT("Snapshot parry succeeds"), Coordinator->GetLastDefenseResult(), EBattleDefenseResult::ParrySuccess);
	TestFalse(TEXT("Enemy action clears snapshot"), Coordinator->HasActiveEnemyPattern());
	TestTrue(TEXT("Idle pattern ID is empty"), Coordinator->GetActiveEnemyPattern().PatternId.IsNone());
	// A second attack reads the edited asset; timeout uses its 5 damage.
	Coordinator->RequestPlayerBasicAttack();
	AdvanceBattleTime(World, 0.11f);
	AdvanceBattleTime(World, 0.21f);
	TestEqual(TEXT("Edited damage applies on next attack"), PlayerCombatant->GetCurrentHealth(), 95.0f);
	// A longer custom window must not expire at the former fixed 0.6 seconds.
	Pattern->Damage = 37.0f;
	Pattern->DefenseWindowDuration = 0.9f;
	Coordinator->RequestPlayerBasicAttack();
	AdvanceBattleTime(World, 0.11f);
	Pattern->Damage = 1.0f;
	AdvanceBattleTime(World, 0.61f);
	TestEqual(TEXT("Custom window still active"), Coordinator->GetCurrentState(), EBattleState::DefenseWindow);
	AdvanceBattleTime(World, 0.30f);
	TestEqual(TEXT("Snapshot damage survives edit"), PlayerCombatant->GetCurrentHealth(), 58.0f);
	TestFalse(TEXT("Timeout clears snapshot"), Coordinator->HasActiveEnemyPattern());
	Coordinator->OnBattleStateChanged().Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorPatternFallbackTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.PatternFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorPatternFallbackTest::RunTest(const FString& Parameters)
{
	for (int32 Case = 0; Case < 3; ++Case)
	{
		FScopedBattleTestWorld TestWorld;
		UWorld* World = TestWorld.GetWorld();
		UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();
		AActor* Encounter = World->SpawnActor<AActor>();
		APawn* Player = World->SpawnActor<APawn>();
		AActor* Enemy = World->SpawnActor<AActor>();
		UCombatantComponent* PlayerCombatant = AddCombatantComponent(Player);
		UCombatantComponent* EnemyCombatant = AddCombatantComponent(Enemy);
		if (Case == 0)
		{
			EnemyCombatant->EnemyAttackPatterns.Empty();
		}
		else if (Case == 1)
		{
			EnemyCombatant->EnemyAttackPatterns[0]->Damage = -1.0f;
			EnemyCombatant->EnemyAttackPatterns.Add(nullptr);
			AddExpectedError(TEXT("skipping pattern"), EAutomationExpectedErrorFlags::Contains, 4);
		}
		else
		{
			EnemyCombatant->EnemyAttackPatterns[0]->SelectionWeight = 0.0f;
		}
		AddExpectedError(TEXT("no selectable enemy patterns"), EAutomationExpectedErrorFlags::Contains, 2);
		Coordinator->RequestBattleStart(Encounter, Player, Enemy);
		Coordinator->RequestPlayerBasicAttack();
		TestEqual(TEXT("Fallback ID"), Coordinator->GetActiveEnemyPattern().PatternId, FName(TEXT("BasicAttack")));
		CompleteNoInputEnemyTurn(World);
		TestEqual(TEXT("Fallback damage unchanged"), PlayerCombatant->GetCurrentHealth(), 80.0f);
		Coordinator->RequestPlayerBasicAttack();
		// Second selection also warns; destroying participant cancels the scheduled attack.
		EnemyCombatant->DestroyComponent();
		AdvanceToDefenseWindow(World);
		TestFalse(TEXT("Invalid participant clears selection"), Coordinator->HasActiveEnemyPattern());
		TestEqual(TEXT("Safely returned to exploration"), Coordinator->GetCurrentState(), EBattleState::Exploration);
		AdvanceBattleTime(World, 3.0f);
		TestEqual(TEXT("Cancelled timer causes no damage"), PlayerCombatant->GetCurrentHealth(), 80.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleCoordinatorEnemyActionLifecycleTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.EnemyActionLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorEnemyActionLifecycleTest::RunTest(const FString& Parameters)
{
	for (int32 Case = 0; Case < 9; ++Case)
	{
		FScopedBattleTestWorld TestWorld;
		UWorld* World = TestWorld.GetWorld();
		UBattleCoordinator* Battle = TestWorld.GetCoordinator();
		AActor* Encounter = World->SpawnActor<AActor>();
		APawn* Player = World->SpawnActor<APawn>();
		AActor* Enemy = World->SpawnActor<AActor>();
		UCombatantComponent* PlayerStats = AddCombatantComponent(Player);
		UCombatantComponent* EnemyStats = AddCombatantComponent(Enemy);
		if (Case == 7) { EnemyStats->ApplyDamage(60.0f); }
		Battle->RequestBattleStart(Encounter, Player, Enemy);
		Battle->RequestPlayerBasicAttack();
		AdvanceToDefenseWindow(World);
		const int64 Id = Battle->ActiveEnemyActionId;
		// Exercise the animation callback boundary independently of rendering.
		Battle->bWaitingForEnemyAnimation = true;
		TestFalse(TEXT("Stale hit rejected"), Battle->NotifyEnemyActionHit(Id - 1));
		TestFalse(TEXT("Stale completion rejected"), Battle->NotifyEnemyActionFinished(Id - 1, false));
		if (Case == 4 || Case == 5)
		{
			if (Case == 4) { Battle->NotifyEnemyActionFinished(Id, true); }
			else
			{
				AddExpectedError(TEXT("Enemy animation completion timed out"), EAutomationExpectedErrorFlags::Contains, 1);
				Battle->HandleEnemyActionTimeout(Id);
			}
			ExpireDefenseWindow(World);
			TestEqual(TEXT("Interrupted before hit does not damage"), PlayerStats->GetCurrentHealth(), 100.0f);
			TestEqual(TEXT("Interruption returns command turn"), Battle->GetCurrentState(), EBattleState::PlayerCommand);
			continue;
		}
		if (Case == 8)
		{
			EnemyStats->DestroyComponent();
			ExpireDefenseWindow(World);
			TestEqual(TEXT("Invalid participant cancels attack"), Battle->GetCurrentState(), EBattleState::Exploration);
			TestFalse(TEXT("Late callback after cancellation rejected"), Battle->NotifyEnemyActionFinished(Id, false));
			continue;
		}
		if (Case == 0 || Case == 7) { AdvanceBattleTime(World, 0.4f); Battle->RequestParry(); }
		if (Case == 1) { Battle->RequestParry(); }
		if (Case == 2) { Battle->RequestDodge(); }
		if (Case <= 2 || Case == 7)
		{
			TestEqual(TEXT("Buffered input causes no early result"), Battle->GetLastDefenseResult(), EBattleDefenseResult::None);
			TestFalse(TEXT("Accepted input locks further defense"), Battle->CanRequestDefenseCommand(EBattleDefenseCommand::Dodge));
			TestEqual(TEXT("Buffered input causes no early damage"), PlayerStats->GetCurrentHealth(), 100.0f);
		}
		if (Case == 6)
		{
			World->GetTimerManager().ClearTimer(Battle->DefenseWindowTimerHandle);
			AddExpectedError(TEXT("Enemy hit marker/timer missing"), EAutomationExpectedErrorFlags::Contains, 1);
			TestTrue(TEXT("Normal end provides missing-hit fallback"), Battle->NotifyEnemyActionFinished(Id, false));
			TestEqual(TEXT("Fallback hit exactly once"), PlayerStats->GetCurrentHealth(), 80.0f);
		}
		else
		{
			ExpireDefenseWindow(World);
			TestEqual(TEXT("Lethal hit takes precedence over recovery"), Battle->GetCurrentState(), Case==7?EBattleState::Victory:EBattleState::DefenseWindow);
			TestFalse(TEXT("No defense after hit"), Battle->CanRequestDefenseCommand(EBattleDefenseCommand::Parry));
			TestFalse(TEXT("Duplicate hit rejected"), Battle->NotifyEnemyActionHit(Id));
			const auto Expected = Case == 0 || Case == 7 ? EBattleDefenseResult::ParrySuccess
				: Case == 1 ? EBattleDefenseResult::Failed : Case == 2 ? EBattleDefenseResult::DodgeSuccess : EBattleDefenseResult::NoInput;
			TestEqual(TEXT("Hit applies buffered result"), Battle->GetLastDefenseResult(), Expected);
			TestEqual(TEXT("Single correct player damage"), PlayerStats->GetCurrentHealth(), Case == 1 || Case == 3 ? 80.0f : 100.0f);
			if (Case==7) { TestTrue(TEXT("Continue result after lethal counter"),Battle->ContinueAfterVictory()); }
			else { TestTrue(TEXT("Animation completion accepted"), Battle->NotifyEnemyActionFinished(Id, false)); }
		}
		TestFalse(TEXT("Duplicate completion rejected"), Battle->NotifyEnemyActionFinished(Id, false));
		TestEqual(TEXT("Recovery chooses next flow"), Battle->GetCurrentState(), Case == 7 ? EBattleState::Exploration : EBattleState::PlayerCommand);
	}
	return true;
}

#endif
