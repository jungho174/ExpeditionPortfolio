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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorInitialStateTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.InitialState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorInitialStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();

	if (!TestNotNull(TEXT("BattleCoordinator should exist for a Game world"), Coordinator))
	{
		return false;
	}

	TestEqual(
		TEXT("Initial battle state should be Exploration"),
		Coordinator->GetCurrentState(),
		EBattleState::Exploration);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorValidTransitionsTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.ValidTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorValidTransitionsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();

	if (!TestNotNull(TEXT("BattleCoordinator should exist for a Game world"), Coordinator))
	{
		return false;
	}

	TestTrue(
		TEXT("Exploration to BattleIntro should succeed"),
		Coordinator->TryTransitionTo(EBattleState::BattleIntro));
	TestEqual(
		TEXT("State should be BattleIntro after the first transition"),
		Coordinator->GetCurrentState(),
		EBattleState::BattleIntro);

	TestTrue(
		TEXT("BattleIntro to PlayerCommand should succeed"),
		Coordinator->TryTransitionTo(EBattleState::PlayerCommand));
	TestEqual(
		TEXT("State should be PlayerCommand after the second transition"),
		Coordinator->GetCurrentState(),
		EBattleState::PlayerCommand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorInvalidTransitionsTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.InvalidTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorInvalidTransitionsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();

	if (!TestNotNull(TEXT("BattleCoordinator should exist for a Game world"), Coordinator))
	{
		return false;
	}

	AddExpectedError(
		TEXT("Rejected battle state transition"),
		EAutomationExpectedErrorFlags::Contains,
		3);

	TestFalse(
		TEXT("Exploration to PlayerCommand should be rejected"),
		Coordinator->TryTransitionTo(EBattleState::PlayerCommand));
	TestEqual(
		TEXT("A rejected skipped transition should not change the state"),
		Coordinator->GetCurrentState(),
		EBattleState::Exploration);

	TestFalse(
		TEXT("A transition to the current state should be rejected"),
		Coordinator->TryTransitionTo(EBattleState::Exploration));
	TestEqual(
		TEXT("A rejected same-state transition should not change the state"),
		Coordinator->GetCurrentState(),
		EBattleState::Exploration);

	TestTrue(
		TEXT("Exploration to BattleIntro should succeed before testing a reverse transition"),
		Coordinator->TryTransitionTo(EBattleState::BattleIntro));

	TestFalse(
		TEXT("BattleIntro to Exploration should be rejected"),
		Coordinator->TryTransitionTo(EBattleState::Exploration));
	TestEqual(
		TEXT("A rejected reverse transition should not change the state"),
		Coordinator->GetCurrentState(),
		EBattleState::BattleIntro);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorDelegateNotificationsTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.DelegateNotifications",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorDelegateNotificationsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();

	if (!TestNotNull(TEXT("BattleCoordinator should exist for a Game world"), Coordinator))
	{
		return false;
	}

	TArray<TPair<EBattleState, EBattleState>> Notifications;
	const FDelegateHandle DelegateHandle = Coordinator->OnBattleStateChanged().AddLambda(
		[&Notifications](const EBattleState PreviousState, const EBattleState NewState)
		{
			Notifications.Emplace(PreviousState, NewState);
		});

	AddExpectedError(
		TEXT("Rejected battle state transition"),
		EAutomationExpectedErrorFlags::Contains,
		2);

	TestFalse(
		TEXT("An invalid transition should be rejected before valid transitions"),
		Coordinator->TryTransitionTo(EBattleState::PlayerCommand));
	TestTrue(
		TEXT("Exploration to BattleIntro should succeed"),
		Coordinator->TryTransitionTo(EBattleState::BattleIntro));
	TestTrue(
		TEXT("BattleIntro to PlayerCommand should succeed"),
		Coordinator->TryTransitionTo(EBattleState::PlayerCommand));
	TestFalse(
		TEXT("A same-state transition should be rejected after valid transitions"),
		Coordinator->TryTransitionTo(EBattleState::PlayerCommand));

	TestEqual(
		TEXT("The delegate should run exactly once for each successful transition"),
		Notifications.Num(),
		2);

	if (Notifications.Num() == 2)
	{
		TestEqual(
			TEXT("First notification should report Exploration as PreviousState"),
			Notifications[0].Key,
			EBattleState::Exploration);
		TestEqual(
			TEXT("First notification should report BattleIntro as NewState"),
			Notifications[0].Value,
			EBattleState::BattleIntro);
		TestEqual(
			TEXT("Second notification should report BattleIntro as PreviousState"),
			Notifications[1].Key,
			EBattleState::BattleIntro);
		TestEqual(
			TEXT("Second notification should report PlayerCommand as NewState"),
			Notifications[1].Value,
			EBattleState::PlayerCommand);
	}

	Coordinator->OnBattleStateChanged().Remove(DelegateHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorBattleStartRequestTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.BattleStartRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorBattleStartRequestTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedBattleTestWorld TestWorld;
	UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();
	UWorld* World = TestWorld.GetWorld();

	if (!TestNotNull(TEXT("BattleCoordinator should exist for a Game world"), Coordinator)
		|| !TestNotNull(TEXT("Test world should exist"), World))
	{
		return false;
	}

	AActor* EncounterSource = World->SpawnActor<AActor>();
	APawn* PlayerPawn = World->SpawnActor<APawn>();
	AActor* EnemyActor = World->SpawnActor<AActor>();
	APawn* MissingPlayerCombatantPawn = World->SpawnActor<APawn>();
	AActor* MissingEnemyCombatantActor = World->SpawnActor<AActor>();
	AActor* OtherEncounterSource = World->SpawnActor<AActor>();
	APawn* OtherPlayerPawn = World->SpawnActor<APawn>();
	AActor* OtherEnemyActor = World->SpawnActor<AActor>();

	if (!TestNotNull(TEXT("Encounter source should spawn"), EncounterSource)
		|| !TestNotNull(TEXT("Player pawn should spawn"), PlayerPawn)
		|| !TestNotNull(TEXT("Enemy actor should spawn"), EnemyActor)
		|| !TestNotNull(TEXT("Pawn without a combatant should spawn"), MissingPlayerCombatantPawn)
		|| !TestNotNull(TEXT("Enemy without a combatant should spawn"), MissingEnemyCombatantActor)
		|| !TestNotNull(TEXT("Other encounter source should spawn"), OtherEncounterSource)
		|| !TestNotNull(TEXT("Other player pawn should spawn"), OtherPlayerPawn)
		|| !TestNotNull(TEXT("Other enemy actor should spawn"), OtherEnemyActor))
	{
		return false;
	}

	UCombatantComponent* PlayerCombatant = AddCombatantComponent(PlayerPawn);
	UCombatantComponent* EnemyCombatant = AddCombatantComponent(EnemyActor);
	UCombatantComponent* OtherPlayerCombatant = AddCombatantComponent(OtherPlayerPawn);
	UCombatantComponent* OtherEnemyCombatant = AddCombatantComponent(OtherEnemyActor);

	if (!TestNotNull(TEXT("Player combatant should be added"), PlayerCombatant)
		|| !TestNotNull(TEXT("Enemy combatant should be added"), EnemyCombatant)
		|| !TestNotNull(TEXT("Other player combatant should be added"), OtherPlayerCombatant)
		|| !TestNotNull(TEXT("Other enemy combatant should be added"), OtherEnemyCombatant))
	{
		return false;
	}

	TArray<TPair<EBattleState, EBattleState>> Notifications;
	const FDelegateHandle DelegateHandle = Coordinator->OnBattleStateChanged().AddLambda(
		[&Notifications](const EBattleState PreviousState, const EBattleState NewState)
		{
			Notifications.Emplace(PreviousState, NewState);
		});

	AddExpectedError(
		TEXT("Rejected battle start request"),
		EAutomationExpectedErrorFlags::Contains,
		7);

	TestFalse(
		TEXT("A null encounter source should be rejected"),
		Coordinator->RequestBattleStart(nullptr, PlayerPawn, EnemyActor));
	TestFalse(
		TEXT("A null player pawn should be rejected"),
		Coordinator->RequestBattleStart(EncounterSource, nullptr, EnemyActor));
	TestFalse(
		TEXT("A null enemy actor should be rejected"),
		Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, nullptr));
	TestFalse(
		TEXT("The same actor cannot be both player and enemy"),
		Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, PlayerPawn));
	TestFalse(
		TEXT("A player without CombatantComponent should be rejected"),
		Coordinator->RequestBattleStart(EncounterSource, MissingPlayerCombatantPawn, EnemyActor));
	TestFalse(
		TEXT("An enemy without CombatantComponent should be rejected"),
		Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, MissingEnemyCombatantActor));

	TestEqual(
		TEXT("Rejected requests should leave the state at Exploration"),
		Coordinator->GetCurrentState(),
		EBattleState::Exploration);
	TestNull(
		TEXT("Rejected requests should not set an active encounter"),
		Coordinator->GetActiveEncounterSource());
	TestNull(
		TEXT("Rejected requests should not set an active player"),
		Coordinator->GetActivePlayerPawn());
	TestNull(
		TEXT("Rejected requests should not set an active enemy"),
		Coordinator->GetActiveEnemyActor());
	TestNull(
		TEXT("Rejected requests should not set a player combatant"),
		Coordinator->GetPlayerCombatant());
	TestNull(
		TEXT("Rejected requests should not set an enemy combatant"),
		Coordinator->GetEnemyCombatant());
	TestEqual(
		TEXT("Rejected requests should not notify listeners"),
		Notifications.Num(),
		0);

	TestTrue(
		TEXT("A valid battle start request should succeed"),
		Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, EnemyActor));
	TestEqual(
		TEXT("The placeholder BattleIntro should continue to PlayerCommand"),
		Coordinator->GetCurrentState(),
		EBattleState::PlayerCommand);
	TestEqual(
		TEXT("The active encounter should match the accepted source"),
		Coordinator->GetActiveEncounterSource(),
		EncounterSource);
	TestEqual(
		TEXT("The active player should match the accepted pawn"),
		Coordinator->GetActivePlayerPawn(),
		PlayerPawn);
	TestEqual(
		TEXT("The active enemy should match the accepted actor"),
		Coordinator->GetActiveEnemyActor(),
		EnemyActor);
	TestEqual(
		TEXT("The player combatant should match the registered component"),
		Coordinator->GetPlayerCombatant(),
		PlayerCombatant);
	TestEqual(
		TEXT("The enemy combatant should match the registered component"),
		Coordinator->GetEnemyCombatant(),
		EnemyCombatant);

	TestFalse(
		TEXT("A second battle start request should be rejected"),
		Coordinator->RequestBattleStart(OtherEncounterSource, OtherPlayerPawn, OtherEnemyActor));
	TestEqual(
		TEXT("A rejected second request should preserve the active encounter"),
		Coordinator->GetActiveEncounterSource(),
		EncounterSource);
	TestEqual(
		TEXT("A rejected second request should preserve the active player"),
		Coordinator->GetActivePlayerPawn(),
		PlayerPawn);
	TestEqual(
		TEXT("A rejected second request should preserve the active enemy"),
		Coordinator->GetActiveEnemyActor(),
		EnemyActor);
	TestEqual(
		TEXT("A rejected second request should preserve the player combatant"),
		Coordinator->GetPlayerCombatant(),
		PlayerCombatant);
	TestEqual(
		TEXT("A rejected second request should preserve the enemy combatant"),
		Coordinator->GetEnemyCombatant(),
		EnemyCombatant);
	TestEqual(
		TEXT("Only the successful request should notify each successful transition"),
		Notifications.Num(),
		2);

	if (Notifications.Num() == 2)
	{
		TestEqual(
			TEXT("The first battle start notification should begin at Exploration"),
			Notifications[0].Key,
			EBattleState::Exploration);
		TestEqual(
			TEXT("The first battle start notification should enter BattleIntro"),
			Notifications[0].Value,
			EBattleState::BattleIntro);
		TestEqual(
			TEXT("The second battle start notification should begin at BattleIntro"),
			Notifications[1].Key,
			EBattleState::BattleIntro);
		TestEqual(
			TEXT("The second battle start notification should enter PlayerCommand"),
			Notifications[1].Value,
			EBattleState::PlayerCommand);
	}

	Coordinator->OnBattleStateChanged().Remove(DelegateHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorMinimalTurnLoopTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.MinimalTurnLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorMinimalTurnLoopTest::RunTest(const FString& Parameters)
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

	bool bReentrantResult = true;
	TArray<TPair<EBattleState, EBattleState>> Notifications;
	const FDelegateHandle DelegateHandle = Coordinator->OnBattleStateChanged().AddLambda(
		[Coordinator, &bReentrantResult, &Notifications](const EBattleState PreviousState, const EBattleState NewState)
		{
			Notifications.Emplace(PreviousState, NewState);
			if (NewState == EBattleState::PlayerAction)
			{
				bReentrantResult = Coordinator->RequestPlayerBasicAttack();
			}
		});

	AddExpectedError(
		TEXT("Rejected battle command 'RequestPlayerCommand'"),
		EAutomationExpectedErrorFlags::Contains,
		1);

	TestTrue(TEXT("A valid basic attack should begin one round"), Coordinator->RequestPlayerBasicAttack());
	TestFalse(TEXT("A reentrant command should be rejected"), bReentrantResult);
	TestEqual(TEXT("The enemy turn should wait in Telegraph"), Coordinator->GetCurrentState(), EBattleState::EnemyTelegraph);
	TestEqual(TEXT("Player attack should deal damage once"), EnemyCombatant->GetCurrentHealth(), 75.0f);
	TestEqual(TEXT("Telegraph should not damage the player"), PlayerCombatant->GetCurrentHealth(), 100.0f);

	AdvanceToDefenseWindow(World);
	TestEqual(TEXT("Telegraph completion should open DefenseWindow"), Coordinator->GetCurrentState(), EBattleState::DefenseWindow);
	TestTrue(TEXT("Dodge should resolve the defense window"), Coordinator->RequestDodge());
	ExpireDefenseWindow(World);
	TestEqual(TEXT("A successful dodge should return to PlayerCommand"), Coordinator->GetCurrentState(), EBattleState::PlayerCommand);
	TestEqual(TEXT("A successful dodge should prevent enemy damage"), PlayerCombatant->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("One defended round should publish seven state changes"), Notifications.Num(), 7);

	if (Notifications.Num() == 7)
	{
		TestEqual(TEXT("Round should enter PlayerAction"), Notifications[0].Value, EBattleState::PlayerAction);
		TestEqual(TEXT("Player action should enter TurnEnd"), Notifications[1].Value, EBattleState::TurnEnd);
		TestEqual(TEXT("Enemy turn should enter Telegraph"), Notifications[2].Value, EBattleState::EnemyTelegraph);
		TestEqual(TEXT("Telegraph should enter EnemyAction"), Notifications[3].Value, EBattleState::EnemyAction);
		TestEqual(TEXT("Enemy action should open DefenseWindow"), Notifications[4].Value, EBattleState::DefenseWindow);
		TestEqual(TEXT("Defense should enter TurnEnd"), Notifications[5].Value, EBattleState::TurnEnd);
		TestEqual(TEXT("Round should return to PlayerCommand"), Notifications[6].Value, EBattleState::PlayerCommand);
	}

	Coordinator->OnBattleStateChanged().Remove(DelegateHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnTransitionDelayTest,
	"ExpeditionPortfolio.Combat.Turn.TransitionDelay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurnTransitionDelayTest::RunTest(const FString& Parameters)
{
	for(int32 Case=0;Case<3;++Case)
	{
		FScopedBattleTestWorld Scope(1.0f);auto* World=Scope.GetWorld();auto* Battle=Scope.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>();auto* Enemy=World->SpawnActor<AActor>();auto* Encounter=World->SpawnActor<AActor>();
		AddCombatantComponent(Player);auto* EnemyHP=AddCombatantComponent(Enemy);
		Battle->RequestBattleStart(Encounter,Player,Enemy);
		if(Case==2){EnemyHP->ApplyDamage(99);}
		Battle->RequestPlayerBasicAttack();
		if(Case==2){TestEqual(TEXT("Lethal hit opens result without opponent turn delay"),Battle->GetCurrentState(),EBattleState::Victory);continue;}
		TestEqual(TEXT("Action finishes into turn gap"),Battle->GetCurrentState(),EBattleState::TurnEnd);
		TestFalse(TEXT("No command during gap"),Battle->RequestPlayerBasicAttack());
		AdvanceBattleTime(World,.99f);TestEqual(TEXT("No next turn before one second"),Battle->GetCurrentState(),EBattleState::TurnEnd);
		if(Case==1){Enemy->Destroy();AdvanceBattleTime(World,.02f);TestEqual(TEXT("Invalid participant cancels delayed turn"),Battle->GetCurrentState(),EBattleState::Exploration);continue;}
		AdvanceBattleTime(World,.02f);TestEqual(TEXT("Enemy turn starts after gap"),Battle->GetCurrentState(),EBattleState::EnemyTelegraph);
		CompleteNoInputEnemyTurn(World);
		TestEqual(TEXT("Enemy completion also waits"),Battle->GetCurrentState(),EBattleState::TurnEnd);
		AdvanceBattleTime(World,.99f);TestEqual(TEXT("Player menu remains closed during gap"),Battle->GetCurrentState(),EBattleState::TurnEnd);
		AdvanceBattleTime(World,.02f);TestEqual(TEXT("Player returns after gap"),Battle->GetCurrentState(),EBattleState::PlayerCommand);
	}
	return true;
}

#endif
