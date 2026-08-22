// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
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

#endif
