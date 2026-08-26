// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "CoreGlobals.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	class FScopedBattleTestWorld
	{
	public:
		FScopedBattleTestWorld()
		{
			const FName WorldName = MakeUniqueObjectName(
				GetTransientPackage(),
				UWorld::StaticClass(),
				TEXT("BattleCoordinatorTestWorld"));

			World = UWorld::CreateWorld(
				EWorldType::Game,
				false,
				WorldName,
				GetTransientPackage());

			if (World != nullptr)
			{
				++GFrameCounter;
				World->GetTimerManager().Tick(0.0f);
			}
		}

		~FScopedBattleTestWorld()
		{
			if (World != nullptr)
			{
				World->DestroyWorld(false);
				World = nullptr;
			}
		}

		UBattleCoordinator* GetCoordinator() const
		{
			return World != nullptr ? World->GetSubsystem<UBattleCoordinator>() : nullptr;
		}

		UWorld* GetWorld() const
		{
			return World;
		}

	private:
		UWorld* World = nullptr;
	};

	UCombatantComponent* AddCombatantComponent(AActor* Owner)
	{
		UCombatantComponent* Combatant = NewObject<UCombatantComponent>(Owner);
		Owner->AddInstanceComponent(Combatant);
		Combatant->RegisterComponent();
		return Combatant;
	}

	void AdvanceBattleTime(UWorld* World, const float DeltaSeconds)
	{
		++GFrameCounter;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorVictoryCleanupTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.VictoryCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorVictoryCleanupTest::RunTest(const FString& Parameters)
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
	TestTrue(TEXT("First round should succeed"), Coordinator->RequestPlayerBasicAttack());
	CompleteNoInputEnemyTurn(World);
	TestTrue(TEXT("Second round should succeed"), Coordinator->RequestPlayerBasicAttack());
	CompleteNoInputEnemyTurn(World);
	TestTrue(TEXT("Third round should succeed"), Coordinator->RequestPlayerBasicAttack());
	CompleteNoInputEnemyTurn(World);
	TestTrue(TEXT("Lethal fourth attack should complete victory cleanup"), Coordinator->RequestPlayerBasicAttack());

	TestEqual(TEXT("Victory should return to Exploration"), Coordinator->GetCurrentState(), EBattleState::Exploration);
	TestEqual(TEXT("Enemy should be defeated"), EnemyCombatant->GetCurrentHealth(), 0.0f);
	TestEqual(TEXT("Enemy should attack in only three rounds"), PlayerCombatant->GetCurrentHealth(), 40.0f);
	TestNull(TEXT("Victory should clear encounter source"), Coordinator->GetActiveEncounterSource());
	TestNull(TEXT("Victory should clear player"), Coordinator->GetActivePlayerPawn());
	TestNull(TEXT("Victory should clear enemy"), Coordinator->GetActiveEnemyActor());
	TestNull(TEXT("Victory should clear player combatant"), Coordinator->GetPlayerCombatant());
	TestNull(TEXT("Victory should clear enemy combatant"), Coordinator->GetEnemyCombatant());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorDefeatTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.Defeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorDefeatTest::RunTest(const FString& Parameters)
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

	TestTrue(TEXT("Setup damage should leave the player alive"), PlayerCombatant->ApplyDamage(80.0f));
	TestTrue(TEXT("Battle start should accept a living player"), Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, EnemyActor));
	TestTrue(TEXT("The defeating round should begin"), Coordinator->RequestPlayerBasicAttack());
	CompleteNoInputEnemyTurn(World);
	TestEqual(TEXT("Defeated player should leave the battle in Defeat"), Coordinator->GetCurrentState(), EBattleState::Defeat);
	TestEqual(TEXT("Enemy attack should defeat the player"), PlayerCombatant->GetCurrentHealth(), 0.0f);
	TestEqual(TEXT("Player attack should damage the enemy first"), EnemyCombatant->GetCurrentHealth(), 75.0f);
	TestEqual(TEXT("Defeat should retain the active player"), Coordinator->GetActivePlayerPawn(), PlayerPawn);
	TestEqual(TEXT("Defeat should retain the active enemy"), Coordinator->GetActiveEnemyActor(), EnemyActor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorDefeatedParticipantTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.DefeatedParticipant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorDefeatedParticipantTest::RunTest(const FString& Parameters)
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

	TestTrue(TEXT("Setup damage should defeat the enemy"), EnemyCombatant->ApplyDamage(100.0f));
	AddExpectedError(
		TEXT("Rejected battle start request: Player and Enemy must both be able to act"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(
		TEXT("Already defeated participants should be rejected"),
		Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, EnemyActor));
	TestEqual(TEXT("Rejected start should preserve Exploration"), Coordinator->GetCurrentState(), EBattleState::Exploration);
	TestNull(TEXT("Rejected start should not retain encounter context"), Coordinator->GetActiveEncounterSource());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorInvalidActiveParticipantTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.InvalidActiveParticipant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorInvalidActiveParticipantTest::RunTest(const FString& Parameters)
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
	EnemyCombatant->DestroyComponent();

	AddExpectedError(
		TEXT("active battle participants are invalid or defeated"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(TEXT("A command with an invalid participant should be rejected"), Coordinator->RequestPlayerBasicAttack());
	TestEqual(TEXT("Invalid participants should return the battle to Exploration"), Coordinator->GetCurrentState(), EBattleState::Exploration);
	TestNull(TEXT("Invalid participant cleanup should clear encounter context"), Coordinator->GetActiveEncounterSource());
	TestNull(TEXT("Invalid participant cleanup should clear player context"), Coordinator->GetActivePlayerPawn());
	TestNull(TEXT("Invalid participant cleanup should clear enemy context"), Coordinator->GetActiveEnemyActor());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorPlayerCommandsTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.PlayerCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorPlayerCommandsTest::RunTest(const FString& Parameters)
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
		TEXT("Rejected battle command 'RequestPlayerCommand'"),
		EAutomationExpectedErrorFlags::Contains,
		2);

	TestFalse(
		TEXT("Commands should be rejected outside PlayerCommand"),
		Coordinator->RequestPlayerCommand(EBattlePlayerCommand::BasicAttack));
	TestFalse(
		TEXT("Basic attack should not be available before a battle"),
		Coordinator->CanRequestPlayerCommand(EBattlePlayerCommand::BasicAttack));
	TestTrue(TEXT("Battle start should succeed"), Coordinator->RequestBattleStart(EncounterSource, PlayerPawn, EnemyActor));
	TestTrue(
		TEXT("Basic attack should be available at PlayerCommand"),
		Coordinator->CanRequestPlayerCommand(EBattlePlayerCommand::BasicAttack));
	TestTrue(
		TEXT("Power Strike should initially be available"),
		Coordinator->CanRequestPlayerCommand(EBattlePlayerCommand::PowerStrike));
	bool bBasicAttackAvailableAtPlayerCommandEvent = false;
	bool bPowerStrikeAvailableAtPlayerCommandEvent = true;
	const FDelegateHandle AvailabilityHandle = Coordinator->OnBattleStateChanged().AddLambda(
		[Coordinator, &bBasicAttackAvailableAtPlayerCommandEvent, &bPowerStrikeAvailableAtPlayerCommandEvent](
			const EBattleState PreviousState,
			const EBattleState NewState)
		{
			(void)PreviousState;
			if (NewState == EBattleState::PlayerCommand)
			{
				bBasicAttackAvailableAtPlayerCommandEvent =
					Coordinator->CanRequestPlayerCommand(EBattlePlayerCommand::BasicAttack);
				bPowerStrikeAvailableAtPlayerCommandEvent =
					Coordinator->CanRequestPlayerCommand(EBattlePlayerCommand::PowerStrike);
			}
		});

	TestTrue(
		TEXT("Power Strike should begin one round"),
		Coordinator->RequestPlayerCommand(EBattlePlayerCommand::PowerStrike));
	TestEqual(TEXT("Power Strike should deal 40 damage"), EnemyCombatant->GetCurrentHealth(), 60.0f);
	TestEqual(TEXT("The enemy should wait before counterattacking"), PlayerCombatant->GetCurrentHealth(), 100.0f);
	TestFalse(
		TEXT("Power Strike should be unavailable after use"),
		Coordinator->CanRequestPlayerCommand(EBattlePlayerCommand::PowerStrike));
	CompleteNoInputEnemyTurn(World);
	TestEqual(TEXT("The enemy should counterattack after no input"), PlayerCombatant->GetCurrentHealth(), 80.0f);
	TestTrue(
		TEXT("The PlayerCommand event should expose Basic Attack as available"),
		bBasicAttackAvailableAtPlayerCommandEvent);
	TestFalse(
		TEXT("The PlayerCommand event should expose consumed Power Strike as unavailable"),
		bPowerStrikeAvailableAtPlayerCommandEvent);
	TestFalse(
		TEXT("A second Power Strike should be rejected"),
		Coordinator->RequestPlayerCommand(EBattlePlayerCommand::PowerStrike));
	TestEqual(TEXT("A rejected command should not deal damage"), EnemyCombatant->GetCurrentHealth(), 60.0f);

	TestTrue(TEXT("First basic attack should succeed"), Coordinator->RequestPlayerBasicAttack());
	CompleteNoInputEnemyTurn(World);
	TestTrue(TEXT("Second basic attack should succeed"), Coordinator->RequestPlayerBasicAttack());
	CompleteNoInputEnemyTurn(World);
	TestTrue(TEXT("The lethal basic attack should succeed"), Coordinator->RequestPlayerBasicAttack());
	TestEqual(TEXT("Victory should return to Exploration"), Coordinator->GetCurrentState(), EBattleState::Exploration);

	AActor* SecondEncounterSource = World->SpawnActor<AActor>();
	APawn* SecondPlayerPawn = World->SpawnActor<APawn>();
	AActor* SecondEnemyActor = World->SpawnActor<AActor>();
	UCombatantComponent* SecondPlayerCombatant = AddCombatantComponent(SecondPlayerPawn);
	UCombatantComponent* SecondEnemyCombatant = AddCombatantComponent(SecondEnemyActor);
	if (!TestNotNull(TEXT("Second player combatant should exist"), SecondPlayerCombatant)
		|| !TestNotNull(TEXT("Second enemy combatant should exist"), SecondEnemyCombatant))
	{
		return false;
	}

	bool bDefenseResultResetForNewBattle = false;
	const FDelegateHandle DefenseResetHandle = Coordinator->OnDefenseResultChanged().AddLambda(
		[&bDefenseResultResetForNewBattle](const EBattleDefenseResult Result)
		{
			bDefenseResultResetForNewBattle = Result == EBattleDefenseResult::None;
		});
	TestTrue(
		TEXT("A new battle should start after victory cleanup"),
		Coordinator->RequestBattleStart(SecondEncounterSource, SecondPlayerPawn, SecondEnemyActor));
	TestTrue(
		TEXT("A new battle should reset Power Strike availability"),
		Coordinator->CanRequestPlayerCommand(EBattlePlayerCommand::PowerStrike));
	TestEqual(
		TEXT("A new battle should reset the previous defense result"),
		Coordinator->GetLastDefenseResult(),
		EBattleDefenseResult::None);
	TestTrue(TEXT("Defense result reset should be published for the HUD"), bDefenseResultResetForNewBattle);
	Coordinator->OnDefenseResultChanged().Remove(DefenseResetHandle);
	Coordinator->OnBattleStateChanged().Remove(AvailabilityHandle);
	return true;
}

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
	AdvanceBattleTime(World, 0.2f);
	TestTrue(TEXT("A parry on the success boundary should succeed"), Coordinator->RequestParry());
	TestEqual(TEXT("Boundary parry should publish ParrySuccess"), Coordinator->GetLastDefenseResult(), EBattleDefenseResult::ParrySuccess);
	TestEqual(TEXT("Parry should prevent player damage"), PlayerCombatant->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("Parry should counter for 15 damage"), EnemyCombatant->GetCurrentHealth(), 60.0f);
	TestEqual(TEXT("Successful parry should return PlayerCommand"), Coordinator->GetCurrentState(), EBattleState::PlayerCommand);

	TestTrue(TEXT("Second player attack should begin Telegraph"), Coordinator->RequestPlayerBasicAttack());
	AdvanceToDefenseWindow(World);
	AdvanceBattleTime(World, 0.21f);
	TestTrue(TEXT("A late parry input should resolve as failure"), Coordinator->RequestParry());
	TestEqual(TEXT("Late parry should publish Failed"), Coordinator->GetLastDefenseResult(), EBattleDefenseResult::Failed);
	TestEqual(TEXT("Failed parry should apply enemy damage"), PlayerCombatant->GetCurrentHealth(), 80.0f);
	TestEqual(TEXT("Failed parry should not counter"), EnemyCombatant->GetCurrentHealth(), 35.0f);

	TestTrue(TEXT("Third player attack should begin Telegraph"), Coordinator->RequestPlayerBasicAttack());
	AdvanceToDefenseWindow(World);
	TestTrue(
		TEXT("Dodge should be available in DefenseWindow"),
		Coordinator->CanRequestDefenseCommand(EBattleDefenseCommand::Dodge));
	TestTrue(TEXT("Dodge should succeed during DefenseWindow"), Coordinator->RequestDodge());
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
	TestTrue(TEXT("Parry should resolve the lethal counter"), Coordinator->RequestParry());

	TestEqual(TEXT("Parry counter should defeat enemy"), EnemyCombatant->GetCurrentHealth(), 0.0f);
	TestEqual(TEXT("Lethal parry should not damage player"), PlayerCombatant->GetCurrentHealth(), 100.0f);
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

	TestTrue(TEXT("First defense input should resolve"), Coordinator->RequestParry());
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
#endif
