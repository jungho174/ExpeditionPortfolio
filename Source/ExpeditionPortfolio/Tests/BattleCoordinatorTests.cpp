// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BattleCoordinator.h"

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
	AActor* OtherEncounterSource = World->SpawnActor<AActor>();
	APawn* OtherPlayerPawn = World->SpawnActor<APawn>();

	if (!TestNotNull(TEXT("Encounter source should spawn"), EncounterSource)
		|| !TestNotNull(TEXT("Player pawn should spawn"), PlayerPawn)
		|| !TestNotNull(TEXT("Other encounter source should spawn"), OtherEncounterSource)
		|| !TestNotNull(TEXT("Other player pawn should spawn"), OtherPlayerPawn))
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
		3);

	TestFalse(
		TEXT("A null encounter source should be rejected"),
		Coordinator->RequestBattleStart(nullptr, PlayerPawn));
	TestFalse(
		TEXT("A null player pawn should be rejected"),
		Coordinator->RequestBattleStart(EncounterSource, nullptr));
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

	TestTrue(
		TEXT("A valid battle start request should succeed"),
		Coordinator->RequestBattleStart(EncounterSource, PlayerPawn));
	TestEqual(
		TEXT("A valid battle start request should enter BattleIntro"),
		Coordinator->GetCurrentState(),
		EBattleState::BattleIntro);
	TestEqual(
		TEXT("The active encounter should match the accepted source"),
		Coordinator->GetActiveEncounterSource(),
		EncounterSource);
	TestEqual(
		TEXT("The active player should match the accepted pawn"),
		Coordinator->GetActivePlayerPawn(),
		PlayerPawn);

	TestFalse(
		TEXT("A second battle start request should be rejected"),
		Coordinator->RequestBattleStart(OtherEncounterSource, OtherPlayerPawn));
	TestEqual(
		TEXT("A rejected second request should preserve the active encounter"),
		Coordinator->GetActiveEncounterSource(),
		EncounterSource);
	TestEqual(
		TEXT("A rejected second request should preserve the active player"),
		Coordinator->GetActivePlayerPawn(),
		PlayerPawn);
	TestEqual(
		TEXT("Only the successful request should notify listeners"),
		Notifications.Num(),
		1);

	if (Notifications.Num() == 1)
	{
		TestEqual(
			TEXT("The battle start notification should report Exploration as PreviousState"),
			Notifications[0].Key,
			EBattleState::Exploration);
		TestEqual(
			TEXT("The battle start notification should report BattleIntro as NewState"),
			Notifications[0].Value,
			EBattleState::BattleIntro);
	}

	Coordinator->OnBattleStateChanged().Remove(DelegateHandle);
	return true;
}

#endif
