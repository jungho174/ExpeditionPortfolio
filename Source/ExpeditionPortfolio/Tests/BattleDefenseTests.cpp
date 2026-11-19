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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorDefenseOutcomesTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.DefenseOutcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorDefenseOutcomesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();
	UWorld* World = TestWorld.GetWorld();
	AActor* EncounterSource = World->SpawnActor<AActor>();
	APawn* PlayerPawn = World->SpawnActor<APawn>();
	AActor* EnemyActor = World->SpawnActor<AActor>();
	UCombatantComponent* PlayerCombatant = AddCombatantComponent(PlayerPawn);
	UCombatantComponent* EnemyCombatant = AddCombatantComponent(EnemyActor);

	if (!TestNotNull(TEXT("Coordinator should exist"), Coordinator)
		|| !TestNotNull(TEXT("Player combatant should exist"), PlayerCombatant)
		|| !TestNotNull(TEXT("Enemy combatant should exist"), EnemyCombatant))
	{
		return false;
	}

	TestTrue(TEXT("Battle start should succeed"), Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, EnemyActor));

	TestTrue(TEXT("First player attack should begin Telegraph"), Coordinator->RequestPlayerBasicAttack());
	AdvanceToDefenseWindow(World);
	TestTrue(
		TEXT("Parry should be available in DefenseWindow"),
		Coordinator->CanRequestDefenseCommand(EBattleDefenseCommand::Parry));
	AdvanceBattleTime(World, 0.4f);
	TestTrue(TEXT("A parry on the success boundary should succeed"), Coordinator->RequestParry());
	TestEqual(TEXT("Input waits for actual hit"), Coordinator->GetLastDefenseResult(), EBattleDefenseResult::None);
	ExpireDefenseWindow(World);
	TestEqual(TEXT("Boundary parry should publish ParrySuccess"), Coordinator->GetLastDefenseResult(), EBattleDefenseResult::ParrySuccess);
	TestEqual(TEXT("Parry should prevent player damage"), PlayerCombatant->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("Parry should counter for 15 damage"), EnemyCombatant->GetCurrentHealth(), 60.0f);
	TestEqual(TEXT("Successful parry should return PlayerCommand"), Coordinator->GetCurrentState(), EBattleState::PlayerCommand);

	TestTrue(TEXT("Second player attack should begin Telegraph"), Coordinator->RequestPlayerBasicAttack());
	AdvanceToDefenseWindow(World);
	AdvanceBattleTime(World, 0.21f);
	TestTrue(TEXT("An early parry input is accepted as failure"), Coordinator->RequestParry());
	ExpireDefenseWindow(World);
	TestEqual(TEXT("Early parry should publish Failed"), Coordinator->GetLastDefenseResult(), EBattleDefenseResult::Failed);
	TestEqual(TEXT("Failed parry should apply enemy damage"), PlayerCombatant->GetCurrentHealth(), 80.0f);
	TestEqual(TEXT("Failed parry should not counter"), EnemyCombatant->GetCurrentHealth(), 35.0f);

	TestTrue(TEXT("Third player attack should begin Telegraph"), Coordinator->RequestPlayerBasicAttack());
	AdvanceToDefenseWindow(World);
	TestTrue(
		TEXT("Dodge should be available in DefenseWindow"),
		Coordinator->CanRequestDefenseCommand(EBattleDefenseCommand::Dodge));
	TestTrue(TEXT("Dodge should succeed during DefenseWindow"), Coordinator->RequestDodge());
	ExpireDefenseWindow(World);
	TestEqual(TEXT("Dodge should publish DodgeSuccess"), Coordinator->GetLastDefenseResult(), EBattleDefenseResult::DodgeSuccess);
	TestEqual(TEXT("Dodge should prevent player damage"), PlayerCombatant->GetCurrentHealth(), 80.0f);
	TestEqual(TEXT("Dodge should not counter"), EnemyCombatant->GetCurrentHealth(), 10.0f);
	TestEqual(TEXT("Successful dodge should return PlayerCommand"), Coordinator->GetCurrentState(), EBattleState::PlayerCommand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorDefenseNoInputDefeatTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.DefenseNoInputDefeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorDefenseNoInputDefeatTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();
	UWorld* World = TestWorld.GetWorld();
	AActor* EncounterSource = World->SpawnActor<AActor>();
	APawn* PlayerPawn = World->SpawnActor<APawn>();
	AActor* EnemyActor = World->SpawnActor<AActor>();
	UCombatantComponent* PlayerCombatant = AddCombatantComponent(PlayerPawn);
	UCombatantComponent* EnemyCombatant = AddCombatantComponent(EnemyActor);

	if (!TestNotNull(TEXT("Coordinator should exist"), Coordinator)
		|| !TestNotNull(TEXT("Player combatant should exist"), PlayerCombatant)
		|| !TestNotNull(TEXT("Enemy combatant should exist"), EnemyCombatant))
	{
		return false;
	}

	TestTrue(TEXT("Setup damage should leave 20 health"), PlayerCombatant->ApplyDamage(80.0f));
	TestTrue(TEXT("Battle start should succeed"), Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, EnemyActor));
	TestTrue(TEXT("Player attack should begin the enemy turn"), Coordinator->RequestPlayerBasicAttack());
	AdvanceToDefenseWindow(World);
	TestEqual(TEXT("Telegraph should open DefenseWindow"), Coordinator->GetCurrentState(), EBattleState::DefenseWindow);
	ExpireDefenseWindow(World);

	TestEqual(TEXT("Timeout should publish NoInput"), Coordinator->GetLastDefenseResult(), EBattleDefenseResult::NoInput);
	TestEqual(TEXT("No input should defeat the player"), PlayerCombatant->GetCurrentHealth(), 0.0f);
	TestEqual(TEXT("Defeated player should remain in Defeat"), Coordinator->GetCurrentState(), EBattleState::Defeat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorParryCounterVictoryTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.ParryCounterVictory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorParryCounterVictoryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();
	UWorld* World = TestWorld.GetWorld();
	AActor* EncounterSource = World->SpawnActor<AActor>();
	APawn* PlayerPawn = World->SpawnActor<APawn>();
	AActor* EnemyActor = World->SpawnActor<AActor>();
	UCombatantComponent* PlayerCombatant = AddCombatantComponent(PlayerPawn);
	UCombatantComponent* EnemyCombatant = AddCombatantComponent(EnemyActor);

	if (!TestNotNull(TEXT("Coordinator should exist"), Coordinator)
		|| !TestNotNull(TEXT("Player combatant should exist"), PlayerCombatant)
		|| !TestNotNull(TEXT("Enemy combatant should exist"), EnemyCombatant))
	{
		return false;
	}

	TestTrue(TEXT("Setup damage should leave enemy at 40"), EnemyCombatant->ApplyDamage(60.0f));
	TestTrue(TEXT("Battle start should succeed"), Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, EnemyActor));
	TestTrue(TEXT("Basic attack should leave enemy at 15"), Coordinator->RequestPlayerBasicAttack());
	AdvanceToDefenseWindow(World);
	AdvanceBattleTime(World, 0.41f);
	TestTrue(TEXT("Parry should resolve the lethal counter"), Coordinator->RequestParry());
	ExpireDefenseWindow(World);

	TestEqual(TEXT("Parry counter should defeat enemy"), EnemyCombatant->GetCurrentHealth(), 0.0f);
	TestEqual(TEXT("Lethal parry should not damage player"), PlayerCombatant->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("Lethal parry opens victory"),Coordinator->GetCurrentState(),EBattleState::Victory);
	TestEqual(TEXT("Parry counted once"),Coordinator->GetBattleResult().SuccessfulParries,1);
	TestTrue(TEXT("Continue lethal parry victory"),Coordinator->ContinueAfterVictory());
	TestEqual(TEXT("Lethal parry should return Exploration"), Coordinator->GetCurrentState(), EBattleState::Exploration);
	TestNull(TEXT("Victory cleanup should clear encounter"), Coordinator->GetActiveEncounterSource());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorDefenseReentrancyTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.DefenseReentrancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorDefenseReentrancyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();
	UWorld* World = TestWorld.GetWorld();
	AActor* EncounterSource = World->SpawnActor<AActor>();
	APawn* PlayerPawn = World->SpawnActor<APawn>();
	AActor* EnemyActor = World->SpawnActor<AActor>();
	UCombatantComponent* PlayerCombatant = AddCombatantComponent(PlayerPawn);
	UCombatantComponent* EnemyCombatant = AddCombatantComponent(EnemyActor);

	if (!TestNotNull(TEXT("Coordinator should exist"), Coordinator)
		|| !TestNotNull(TEXT("Player combatant should exist"), PlayerCombatant)
		|| !TestNotNull(TEXT("Enemy combatant should exist"), EnemyCombatant))
	{
		return false;
	}

	AddExpectedError(
		TEXT("Rejected battle command 'RequestDefenseCommand'"),
		EAutomationExpectedErrorFlags::Contains,
		2);
	TestTrue(TEXT("Battle start should succeed"), Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, EnemyActor));
	TestFalse(TEXT("Defense input outside DefenseWindow should be rejected"), Coordinator->RequestParry());
	TestTrue(TEXT("Player attack should begin enemy turn"), Coordinator->RequestPlayerBasicAttack());
	AdvanceToDefenseWindow(World);

	bool bReentrantDefenseResult = true;
	const FDelegateHandle DefenseHandle = Coordinator->OnDefenseResultChanged().AddLambda(
		[Coordinator, &bReentrantDefenseResult](const EBattleDefenseResult Result)
		{
			if (Result == EBattleDefenseResult::ParrySuccess)
			{
				bReentrantDefenseResult = Coordinator->RequestDodge();
			}
		});

	AdvanceBattleTime(World, 0.41f);
	TestTrue(TEXT("First defense input should resolve"), Coordinator->RequestParry());
	ExpireDefenseWindow(World);
	TestFalse(TEXT("Reentrant defense input should be rejected"), bReentrantDefenseResult);
	TestEqual(TEXT("Only first result should be retained"), Coordinator->GetLastDefenseResult(), EBattleDefenseResult::ParrySuccess);
	TestEqual(TEXT("Resolved defense should return PlayerCommand"), Coordinator->GetCurrentState(), EBattleState::PlayerCommand);
	Coordinator->OnDefenseResultChanged().Remove(DefenseHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorInvalidTelegraphParticipantTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.InvalidTelegraphParticipant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorInvalidTelegraphParticipantTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();
	UWorld* World = TestWorld.GetWorld();
	AActor* EncounterSource = World->SpawnActor<AActor>();
	APawn* PlayerPawn = World->SpawnActor<APawn>();
	AActor* EnemyActor = World->SpawnActor<AActor>();
	UCombatantComponent* PlayerCombatant = AddCombatantComponent(PlayerPawn);
	UCombatantComponent* EnemyCombatant = AddCombatantComponent(EnemyActor);

	if (!TestNotNull(TEXT("Coordinator should exist"), Coordinator)
		|| !TestNotNull(TEXT("Player combatant should exist"), PlayerCombatant)
		|| !TestNotNull(TEXT("Enemy combatant should exist"), EnemyCombatant))
	{
		return false;
	}

	TestTrue(TEXT("Battle start should succeed"), Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, EnemyActor));
	TestTrue(TEXT("Player attack should schedule Telegraph"), Coordinator->RequestPlayerBasicAttack());
	EnemyCombatant->DestroyComponent();
	AdvanceToDefenseWindow(World);

	TestEqual(TEXT("Invalid timer participant should return Exploration"), Coordinator->GetCurrentState(), EBattleState::Exploration);
	TestNull(TEXT("Invalid timer participant should clear encounter"), Coordinator->GetActiveEncounterSource());
	AdvanceBattleTime(World, 1.0f);
	TestEqual(TEXT("Cleared timer should not change later state"), Coordinator->GetCurrentState(), EBattleState::Exploration);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleDefensePresentationSignalTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.DefensePresentationSignal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBattleDefensePresentationSignalTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	UBattleCoordinator* Battle = TestWorld.GetCoordinator();
	AActor* Encounter = World->SpawnActor<AActor>();
	APawn* Player = World->SpawnActor<APawn>();
	AActor* Enemy = World->SpawnActor<AActor>();
	UCombatantComponent* PlayerStats = AddCombatantComponent(Player);
	AddCombatantComponent(Enemy);
	Battle->RequestBattleStart(Encounter, Player, Enemy);
	Battle->RequestPlayerBasicAttack();
	AdvanceToDefenseWindow(World);
	int32 Signals = 0;
	bool bReentrantAccepted = false;
	const auto Handle = Battle->OnDefenseCommandAccepted().AddLambda([&](EBattleDefenseCommand Command, float Remaining)
	{
		++Signals;
		TestEqual(TEXT("Accepted command reaches presentation"), Command, EBattleDefenseCommand::Dodge);
		TestTrue(TEXT("Presentation receives pending hit time"), Remaining > 0.0f);
		bReentrantAccepted = Battle->RequestParry();
	});
	AddExpectedError(TEXT("Rejected battle command 'RequestDefenseCommand'"), EAutomationExpectedErrorFlags::Contains, 2);
	TestTrue(TEXT("First dodge accepted"), Battle->RequestDodge());
	TestFalse(TEXT("Presentation cannot reenter defense"), bReentrantAccepted);
	TestFalse(TEXT("Repeated input rejected"), Battle->RequestDodge());
	TestEqual(TEXT("Presentation notified exactly once"), Signals, 1);
	TestEqual(TEXT("Presentation event does not apply early damage"), PlayerStats->GetCurrentHealth(), 100.0f);
	ExpireDefenseWindow(World);
	TestEqual(TEXT("Presentation leaves game outcome unchanged"), Battle->GetLastDefenseResult(), EBattleDefenseResult::DodgeSuccess);
	Battle->OnDefenseCommandAccepted().Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryCounterLifecycleTest,
	"ExpeditionPortfolio.Combat.ParryCounter.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FParryCounterLifecycleTest::RunTest(const FString& Parameters)
{
	for (int32 Case = 0; Case < 6; ++Case)
	{
		FScopedBattleTestWorld TestWorld; auto* World = TestWorld.GetWorld(); auto* Battle = TestWorld.GetCoordinator();
		auto* Player = World->SpawnActor<APawn>(); auto* Enemy = World->SpawnActor<AActor>();
		auto* PlayerHP = AddCombatantComponent(Player); auto* HP = AddCombatantComponent(Enemy);
		Battle->RequestBattleStart(World->SpawnActor<AActor>(), Player, Enemy);
		Battle->RequestPlayerBasicAttack(); AdvanceToDefenseWindow(World);
		const int64 Id = Battle->ActiveEnemyActionId;
		World->GetTimerManager().ClearTimer(Battle->DefenseWindowTimerHandle);
		Battle->bEnemyHitResolved = true;
		Battle->bParryCounterActive = true;
		Battle->PendingCounterDamage = 15;
		Battle->bWaitingForEnemyAnimation = true;
		TestFalse(TEXT("Stale impact rejected"), Battle->NotifyParryCounterHit(Id - 1));
		TestTrue(TEXT("Enemy completion waits for counter"), Battle->NotifyEnemyActionFinished(Id, false));
		TestEqual(TEXT("No premature command turn"), Battle->GetCurrentState(), EBattleState::DefenseWindow);
		TestEqual(TEXT("No counter damage before impact"), HP->GetCurrentHealth(), 75.0f);
		if (Case == 2) { HP->ApplyDamage(60); }
		if (Case == 3)
		{
			AddExpectedError(TEXT("Parry counter timed out"), EAutomationExpectedErrorFlags::Contains, 1);
			Battle->HandleParryCounterTimeout(Id);
			TestEqual(TEXT("Timeout cancels unapplied damage"), HP->GetCurrentHealth(), 75.0f);
		}
		else if (Case == 4)
		{
			HP->DestroyComponent();
			TestFalse(TEXT("Invalid participant rejects counter impact"), Battle->NotifyParryCounterHit(Id));
			TestEqual(TEXT("Invalid participant safely leaves battle"), Battle->GetCurrentState(), EBattleState::Exploration);
		}
		else
		{
			if (Case != 1)
			{
				bool Reentered = true;
				HP->OnHealthChanged().AddLambda([&](float, float) { Reentered = Battle->NotifyParryCounterFinished(Id, false); });
				TestTrue(TEXT("Counter impact accepted"), Battle->NotifyParryCounterHit(Id));
				TestFalse(TEXT("Impact event reentry rejected"), Reentered);
				TestFalse(TEXT("Duplicate impact rejected"), Battle->NotifyParryCounterHit(Id));
				TestEqual(TEXT("Lethal counter starts result at impact"), Battle->GetCurrentState(), Case==2?EBattleState::Victory:EBattleState::DefenseWindow);
			}
			if (Case==2) { TestTrue(TEXT("Continue counter result"),Battle->ContinueAfterVictory()); }
			else { TestTrue(TEXT("Counter completes"), Battle->NotifyParryCounterFinished(Id, Case == 1 || Case == 5)); }
			TestEqual(TEXT("Exactly one hit or interruption cancellation"), HP->GetCurrentHealth(), Case == 1 ? 75.0f : Case == 2 ? 0.0f : 60.0f);
		}
		TestFalse(TEXT("Counter state cleaned"), Battle->IsParryCounterActive());
		TestFalse(TEXT("Late completion rejected"), Battle->NotifyParryCounterFinished(Id, false));
		TestEqual(TEXT("Parry counter never damages player"), PlayerHP->GetCurrentHealth(), 100.0f);
		if (Case != 4) { TestEqual(TEXT("Returns command or victory exploration"), Battle->GetCurrentState(), Case == 2 ? EBattleState::Exploration : EBattleState::PlayerCommand); }
	}
	return true;
}

#endif
