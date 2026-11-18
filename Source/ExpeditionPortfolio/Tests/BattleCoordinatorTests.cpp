// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BattleCoordinator.h"
#include "Combat/ActionQTESubsystem.h"
#include "Combat/CombatantComponent.h"
#include "Combat/PlayerCommandData.h"
#include "Combat/PartyHealthSubsystem.h"
#include "Engine/GameInstance.h"
#include "Exploration/Encounters/BattleEncounterTrigger.h"
#include "Exploration/Encounters/EncounterProgressSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "UI/MagicActionData.h"
#include "UI/MagicPresentationComponent.h"
#include "UI/BattleCameraRig.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include <limits>

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "CoreGlobals.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

class FBattleCoordinatorTestAccess
{
public:
	static void SetTurnDelay(UBattleCoordinator* Battle,float Delay) { Battle->TestTurnTransitionDelay=Delay;Battle->bContentFreeTest=true; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyComboLifecycleTest,
	"ExpeditionPortfolio.Combat.EnemyCombo.Lifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FEnemyComboLifecycleTest::RunTest(const FString& Parameters)
{
	for (int32 Case=0;Case<7;++Case)
	{
		const FName WorldName=MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("ComboTest"));
		UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,WorldName,GetTransientPackage());
		auto* Battle=World->GetSubsystem<UBattleCoordinator>(); FBattleCoordinatorTestAccess::SetTurnDelay(Battle,0);
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>();
		auto AddStats=[](AActor* Owner) { auto* Stats=NewObject<UCombatantComponent>(Owner); Owner->AddInstanceComponent(Stats); Stats->RegisterComponent(); return Stats; };
		auto* PlayerHP=AddStats(Player); auto* EnemyHP=AddStats(Enemy);
		auto* Pattern=NewObject<UEnemyAttackPatternData>(Enemy); Pattern->PatternId=TEXT("Triple");
		for (float Time:{.6f,1.3f,2.f}) { FEnemyAttackHit Hit; Hit.Damage=10; Hit.FallbackHitTime=Time; Pattern->Hits.Add(Hit); }
		EnemyHP->EnemyAttackPatterns.Add(Pattern);
		auto Advance=[&](float Time) { ++GFrameCounter; World->TimeSeconds+=Time; World->GetTimerManager().Tick(Time); };
		Advance(0);
		Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy); Battle->RequestPlayerBasicAttack(); Advance(1.01f);
		const int64 Id=Battle->ActiveEnemyActionId;
		TestEqual(TEXT("Three distinct hits copied"),Battle->GetEnemyHitCount(),3);
		TestFalse(TEXT("Future indexed marker cannot hit current window"),Battle->NotifyEnemyActionHit(Id,1));
		if (Case==6) { Pattern->Hits[1].Damage=99; Pattern->Hits[1].FallbackHitTime=20; }
		int32 Parries=0;
		Battle->OnDefenseResultChanged().AddLambda([&](EBattleDefenseResult Result) { if (Result==EBattleDefenseResult::ParrySuccess) { ++Parries; } });
		for (int32 Index=0;Index<3;++Index)
		{
			Advance(Index==0?.49f:.58f);
			const bool bMiss=Case==4 || (Case==1 && Index==1);
			if (!bMiss)
			{
				const bool bDodge=Case==2 && Index==1;
				TestTrue(TEXT("New hit accepts a fresh defense"),bDodge?Battle->RequestDodge():Battle->RequestParry());
				TestFalse(TEXT("Repeat input in same hit rejected"),Battle->RequestParry());
			}
			Advance(.12f);
			TestFalse(TEXT("Previous indexed marker rejected"),Battle->NotifyEnemyActionHit(Id,Index));
			if (Case==3 && Index==0)
			{
				TestTrue(TEXT("Interruption accepted"),Battle->NotifyEnemyActionFinished(Id,true)); Advance(5);
				TestEqual(TEXT("Remaining interrupted hits cancelled"),PlayerHP->GetCurrentHealth(),100.f); break;
			}
			if (Case==5 && Index==0) { PlayerHP->ApplyDamage(95); }
			if (Case==5 && Index==1) { Advance(3); break; }
		}
		if (Case==0 || Case==6) { TestEqual(TEXT("Only complete parry earns one counter"),EnemyHP->GetCurrentHealth(),60.f); TestEqual(TEXT("Every parry emits feedback"),Parries,3); }
		if (Case==1) { TestEqual(TEXT("Middle miss damages once using copied data"),PlayerHP->GetCurrentHealth(),90.f); TestEqual(TEXT("Partial parry earns no counter"),EnemyHP->GetCurrentHealth(),75.f); }
		if (Case==2) { TestEqual(TEXT("Mixed dodge avoids damage"),PlayerHP->GetCurrentHealth(),100.f); TestEqual(TEXT("Mixed dodge earns no counter"),EnemyHP->GetCurrentHealth(),75.f); }
		if (Case==4) { TestEqual(TEXT("Each unguarded hit damages once"),PlayerHP->GetCurrentHealth(),70.f); }
		if (Case!=5) { TestEqual(TEXT("Combo ends in command turn"),Battle->GetCurrentState(),EBattleState::PlayerCommand); }
		World->DestroyWorld(false);
	}
	return true;
}

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
				FBattleCoordinatorTestAccess::SetTurnDelay(World->GetSubsystem<UBattleCoordinator>(),0);
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
	TestEqual(TEXT("Victory holds the last scene until confirmed"),Coordinator->GetCurrentState(),EBattleState::Victory);
	TestEqual(TEXT("Actual damage total"),Coordinator->GetBattleResult().DamageDealt,100.f);
	TestEqual(TEXT("Received damage total"),Coordinator->GetBattleResult().DamageReceived,60.f);
	TestEqual(TEXT("Enemy is defeated before result dismissal"),EnemyCombatant->GetCurrentHealth(),0.f);
	TestTrue(TEXT("Continue releases battle context"),Coordinator->ContinueAfterVictory());

	TestEqual(TEXT("Victory should return to Exploration"), Coordinator->GetCurrentState(), EBattleState::Exploration);
	TestTrue(TEXT("Enemy is destroyed after result dismissal"),EnemyActor->IsActorBeingDestroyed());
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
	TestTrue(TEXT("Dismiss victory before a new encounter"),Coordinator->ContinueAfterVictory());
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBattleCoordinatorPlayerActionLifecycleTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.PlayerActionLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleCoordinatorPlayerActionLifecycleTest::RunTest(const FString& Parameters)
{
	// Exercise the real asynchronous signal boundary independently of rendering/Montage assets.
	for (int32 Case = 0; Case < 6; ++Case)
	{
		FScopedBattleTestWorld TestWorld;
		UWorld* World = TestWorld.GetWorld();
		UBattleCoordinator* Coordinator = TestWorld.GetCoordinator();
		AActor* Encounter = World->SpawnActor<AActor>();
		APawn* Player = World->SpawnActor<APawn>();
		AActor* Enemy = World->SpawnActor<AActor>();
		UCombatantComponent* PlayerCombatant = AddCombatantComponent(Player);
		UCombatantComponent* EnemyCombatant = AddCombatantComponent(Enemy);
		Coordinator->RequestBattleStart(Encounter, Player, Enemy);
		Coordinator->TryTransitionTo(EBattleState::PlayerAction);
		Coordinator->ActivePlayerActionId = 41;
		Coordinator->PendingPlayerDamage = 25.0f;
		Coordinator->bPlayerHitApplied = false;
		TestFalse(TEXT("Old action cannot hit"), Coordinator->NotifyPlayerActionHit(40));
		TestFalse(TEXT("Old action cannot complete"), Coordinator->NotifyPlayerActionFinished(40, false));
		TestEqual(TEXT("No early damage"), EnemyCombatant->GetCurrentHealth(), 100.0f);
		AddExpectedError(TEXT("current state is not PlayerCommand"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("Repeated attack locked during animation"), Coordinator->RequestPlayerBasicAttack());
		if (Case == 0 || Case == 3)
		{
			bool bReentrantFinish = true;
			const FDelegateHandle Handle = EnemyCombatant->OnHealthChanged().AddLambda(
				[&](float Previous, float Current) { bReentrantFinish = Coordinator->NotifyPlayerActionFinished(41, false); });
			TestTrue(TEXT("First hit accepted"), Coordinator->NotifyPlayerActionHit(41));
			TestFalse(TEXT("Reentrant finish rejected"), bReentrantFinish);
			TestFalse(TEXT("Duplicate hit rejected"), Coordinator->NotifyPlayerActionHit(41));
			EnemyCombatant->OnHealthChanged().Remove(Handle);
			TestEqual(TEXT("Wait for animation after hit"), Coordinator->GetCurrentState(), EBattleState::PlayerAction);
		}
		if (Case == 2)
		{
			TestTrue(TEXT("Interruption resolves without damage"), Coordinator->NotifyPlayerActionFinished(41, true));
			TestEqual(TEXT("No fabricated interruption hit"), EnemyCombatant->GetCurrentHealth(), 100.0f);
		}
		else if (Case == 3)
		{
			AddExpectedError(TEXT("completion timed out"), EAutomationExpectedErrorFlags::Contains, 1);
			World->GetTimerManager().SetTimer(Coordinator->PlayerActionTimerHandle,
				FTimerDelegate::CreateUObject(Coordinator, &UBattleCoordinator::HandlePlayerActionTimeout, int64(41)), 0.5f, false);
			AdvanceBattleTime(World, 0.51f);
			TestEqual(TEXT("Timeout after hit does not hit twice"), EnemyCombatant->GetCurrentHealth(), 75.0f);
		}
		else if (Case == 4)
		{
			EnemyCombatant->DestroyComponent();
			TestFalse(TEXT("Invalid target cancels safely"), Coordinator->NotifyPlayerActionHit(41));
			TestEqual(TEXT("Invalid target returns exploration"), Coordinator->GetCurrentState(), EBattleState::Exploration);
		}
		else
		{
			if (Case == 5)
			{
				EnemyCombatant->ApplyDamage(75.0f);
				Coordinator->NotifyPlayerActionHit(41);
				TestEqual(TEXT("Lethal hit starts result immediately"), Coordinator->GetCurrentState(), EBattleState::Victory);
				TestTrue(TEXT("Continue lethal result"),Coordinator->ContinueAfterVictory());
			}
			if (Case!=5) { TestTrue(TEXT("Normal completion including missing notify"), Coordinator->NotifyPlayerActionFinished(41, false)); }
			TestEqual(TEXT("Exactly one hit"), EnemyCombatant->GetCurrentHealth(), Case == 5 ? 0.0f : 75.0f);
		}
		TestEqual(TEXT("Action context cleared"), Coordinator->ActivePlayerActionId, int64(0));
		TestFalse(TEXT("Late hit rejected"), Coordinator->NotifyPlayerActionHit(41));
		TestFalse(TEXT("Duplicate completion rejected"), Coordinator->NotifyPlayerActionFinished(41, false));
		TestEqual(TEXT("Next flow reached"), Coordinator->GetCurrentState(),
			(Case == 4 || Case == 5) ? EBattleState::Exploration : EBattleState::EnemyTelegraph);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerPotionCommandsTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.PotionCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerPotionCommandsTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	auto* Coordinator = TestWorld.GetCoordinator();
	auto* Player = World->SpawnActor<APawn>();
	auto* Enemy = World->SpawnActor<AActor>();
	auto* Source = World->SpawnActor<AActor>();
	auto* PlayerHP = AddCombatantComponent(Player);
	auto* EnemyHP = AddCombatantComponent(Enemy);
	TestTrue(TEXT("Start encounter"), Coordinator->RequestBattleStart(Source, Player, Enemy));
	TestTrue(TEXT("Full health potion available"), Coordinator->CanRequestPlayerCommand(EBattlePlayerCommand::UsePotion));
	TestEqual(TEXT("Availability does not consume stock"), Coordinator->GetRemainingPotions(), 3);
	TestEqual(TEXT("Availability keeps turn"), Coordinator->GetCurrentState(), EBattleState::PlayerCommand);
	PlayerHP->ApplyDamage(50);
	int32 Heals = 0;
	PlayerHP->OnHealthChanged().AddLambda([&](float Before, float After)
	{
		if (After > Before)
		{
			++Heals;
			TestFalse(TEXT("Healing event cannot reenter commands"), Coordinator->RequestPlayerCommand(EBattlePlayerCommand::UsePotion));
		}
	});
	for (int32 I=0; I<3; ++I)
	{
		const float Before = PlayerHP->GetCurrentHealth();
		TestTrue(TEXT("Potion accepted"), Coordinator->RequestPlayerCommand(EBattlePlayerCommand::UsePotion));
		TestEqual(TEXT("Healing clamps at maximum"), PlayerHP->GetCurrentHealth(), FMath::Min(100.0f, Before + 30));
		TestEqual(TEXT("Consumes one potion"), Coordinator->GetRemainingPotions(), 2-I);
		TestEqual(TEXT("Consumes player turn"), Coordinator->GetCurrentState(), EBattleState::EnemyTelegraph);
		TestFalse(TEXT("Repeated command rejected"), Coordinator->RequestPlayerCommand(EBattlePlayerCommand::UsePotion));
		CompleteNoInputEnemyTurn(World);
	}
	TestEqual(TEXT("One healing event per item"), Heals, 3);
	TestFalse(TEXT("Empty stock rejected"), Coordinator->RequestPlayerCommand(EBattlePlayerCommand::UsePotion));
	TestEqual(TEXT("Potion never damages enemy"), EnemyHP->GetCurrentHealth(), 100.0f);
	TestTrue(TEXT("Melee is separate command"), Coordinator->RequestPlayerCommand(EBattlePlayerCommand::MeleeAttack));
	TestEqual(TEXT("Melee applies configured default damage"), EnemyHP->GetCurrentHealth(), 85.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerCommandDataTest,
	"ExpeditionPortfolio.Combat.BattleCoordinator.PlayerCommandData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerCommandDataTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld TestWorld;
	auto* World = TestWorld.GetWorld();
	auto* Player = World->SpawnActor<APawn>();
	auto* Enemy = World->SpawnActor<AActor>();
	auto* Source = World->SpawnActor<AActor>();
	auto* HP = AddCombatantComponent(Player);
	auto* EnemyHP = AddCombatantComponent(Enemy);
	auto* Data = NewObject<UPlayerCommandData>(Player);
	Data->MeleeDamage = 7;
	Data->PotionHealing = 12;
	Data->PotionsPerEncounter = 2;
	HP->PlayerCommandData = Data;
	auto* Coordinator = TestWorld.GetCoordinator();
	TestTrue(TEXT("Valid editor tuning"), Data->IsValidConfiguration());
	TestTrue(TEXT("Start"), Coordinator->RequestBattleStart(Source, Player, Enemy));
	Data->PotionHealing = 99;
	Data->MeleeDamage = 99;
	TestEqual(TEXT("Stock from data"), Coordinator->GetRemainingPotions(), 2);
	TestEqual(TEXT("Rules snapshotted for encounter"), Coordinator->GetPotionHealing(), 12.0f);
	TestTrue(TEXT("Melee"), Coordinator->RequestPlayerCommand(EBattlePlayerCommand::MeleeAttack));
	TestEqual(TEXT("Melee uses encounter snapshot"), EnemyHP->GetCurrentHealth(), 93.0f);
	CompleteNoInputEnemyTurn(World);
	TestTrue(TEXT("Potion"), Coordinator->RequestPlayerCommand(EBattlePlayerCommand::UsePotion));
	TestEqual(TEXT("Healing uses encounter snapshot"), HP->GetCurrentHealth(), 92.0f);
	Data->PotionHealing = -1;
	TestFalse(TEXT("Invalid settings detected"), Data->IsValidConfiguration());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMagicProjectileDataTest,
	"ExpeditionPortfolio.Combat.MagicProjectile.DataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMagicProjectileDataTest::RunTest(const FString& Parameters)
{
	auto* Data = NewObject<UMagicActionData>();
	TestEqual(TEXT("Non-projectile has no flight"), Data->GetFlightDuration(450), 0.0f);
	Data->bUseProjectile = true;
	TestTrue(TEXT("Default flight settings valid"), Data->ValidateProjectile());
	TestEqual(TEXT("Distance/speed gives flight time"), Data->GetFlightDuration(450), 0.3f);
	TestEqual(TEXT("Overlapping targets still have deferred arrival"), Data->GetFlightDuration(0), 0.05f);
	TestEqual(TEXT("Very distant target bounded by watchdog-compatible maximum"), Data->GetFlightDuration(100000), 3.0f);
	Data->ProjectileSpeed = 0;
	TestFalse(TEXT("Zero speed rejected"), Data->ValidateProjectile());
	TestEqual(TEXT("Invalid data cannot divide by zero"), Data->GetFlightDuration(100), 0.0f);
	Data->ProjectileSpeed = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN speed rejected"), Data->ValidateProjectile());
	Data->ProjectileSpeed = 1000;
	Data->MaxFlightDuration = 11;
	TestFalse(TEXT("Unbounded flight rejected"), Data->ValidateProjectile());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMagicProjectileLifecycleTest,
	"ExpeditionPortfolio.Combat.MagicProjectile.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMagicProjectileLifecycleTest::RunTest(const FString& Parameters)
{
	// Exercise the real travel/update and Coordinator boundary without requiring a renderer.
	for (int32 Case = 0; Case < 6; ++Case)
	{
		FScopedBattleTestWorld TestWorld;
		auto* World = TestWorld.GetWorld();
		auto* Battle = TestWorld.GetCoordinator();
		auto* Player = World->SpawnActor<ACharacter>();
		auto* Enemy = World->SpawnActor<ACharacter>();
		auto* Encounter = World->SpawnActor<AActor>();
		AddCombatantComponent(Player);
		auto* EnemyHP = AddCombatantComponent(Enemy);
		Enemy->SetActorLocation(FVector(1000, 0, 0));
		Battle->RequestBattleStart(Encounter, Player, Enemy);
		Battle->TryTransitionTo(EBattleState::PlayerAction);
		Battle->ActivePlayerActionId = 91;
		Battle->PendingPlayerDamage = 25;
		Battle->bPlayerHitApplied = false;
		auto* Presentation = NewObject<UMagicPresentationComponent>(Player);
		Player->AddInstanceComponent(Presentation);
		Presentation->RegisterComponent();
		Battle->PlayerPresentation = Presentation;
		Presentation->ActiveId = 91;
		Presentation->Mesh = Player->GetMesh();
		Presentation->TargetActor = Enemy;
		Presentation->ActiveData = NewObject<UMagicActionData>(Presentation);
		Presentation->ActiveData->bUseProjectile = true;
		Presentation->ActiveData->ProjectileSpeed = 1000;
		// NAME_None uses the component origin; the test character needs no skeleton assets.
		Presentation->ActiveData->CastSocket = NAME_None;
		if (Case == 5)
		{
			AddExpectedError(TEXT("launch marker missing"), EAutomationExpectedErrorFlags::Contains, 1);
			Presentation->OnMontageEnded(nullptr, false, 91);
			TestTrue(TEXT("Missing marker launches instead of inventing instant damage"), Presentation->bProjectileInFlight);
		}
		else { TestTrue(TEXT("Launch accepted"), Presentation->LaunchProjectile()); }
		TestFalse(TEXT("Duplicate launch rejected"), Presentation->LaunchProjectile());
		Presentation->TickComponent(2.0f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("A hitch in the launch frame is not pre-launch flight time"), Presentation->FlightElapsed, 0.0f);
		TestEqual(TEXT("Launch does not damage target"), EnemyHP->GetCurrentHealth(), 100.0f);
		Presentation->UpdateProjectile(0.4f);
		TestEqual(TEXT("No damage while travelling"), EnemyHP->GetCurrentHealth(), 100.0f);
		if (Case == 2)
		{
			Presentation->OnMontageBlendingOut(nullptr, true, 91);
			Presentation->UpdateProjectile(2);
			TestEqual(TEXT("Interrupted flight never applies damage"), EnemyHP->GetCurrentHealth(), 100.0f);
		}
		else if (Case == 3)
		{
			Enemy->Destroy();
			Presentation->UpdateProjectile(2);
			TestEqual(TEXT("Invalid target leaves combat safely"), Battle->GetCurrentState(), EBattleState::Exploration);
		}
		else if (Case == 4)
		{
			AddExpectedError(TEXT("completion timed out"), EAutomationExpectedErrorFlags::Contains, 1);
			Battle->HandlePlayerActionTimeout(91);
			Presentation->UpdateProjectile(2);
			TestEqual(TEXT("Watchdog cancels pending impact"), EnemyHP->GetCurrentHealth(), 100.0f);
		}
		else
		{
			if (Case == 1) { Presentation->OnMontageEnded(nullptr, false, 91); }
			TestEqual(TEXT("Early animation completion waits for arrival"), Battle->GetCurrentState(), EBattleState::PlayerAction);
			Presentation->UpdateProjectile(0.7f);
			TestEqual(TEXT("Arrival applies exactly 25"), EnemyHP->GetCurrentHealth(), 75.0f);
			Presentation->UpdateProjectile(1);
			TestEqual(TEXT("Repeated update cannot hit twice"), EnemyHP->GetCurrentHealth(), 75.0f);
			if (Case == 0)
			{
				TestEqual(TEXT("Early arrival waits for animation"), Battle->GetCurrentState(), EBattleState::PlayerAction);
				Presentation->OnMontageEnded(nullptr, false, 91);
			}
		}
		TestFalse(TEXT("No remaining travel tick"), Presentation->IsComponentTickEnabled());
		TestEqual(TEXT("Context cleaned once"), Battle->ActivePlayerActionId, int64(0));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryImpactRestoreTest,
	"ExpeditionPortfolio.Combat.ParryCounter.ImpactRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FParryImpactRestoreTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld TestWorld; auto* World = TestWorld.GetWorld();
	auto* Owner = World->SpawnActor<APawn>(); auto* Target = World->SpawnActor<AActor>();
	auto* Settings = World->SpawnActor<AWorldSettings>();
	auto* Magic = NewObject<UMagicPresentationComponent>(Owner);
	Owner->AddInstanceComponent(Magic); Magic->RegisterComponent();
	auto* Mesh = NewObject<USkeletalMeshComponent>(Owner);
	Owner->AddInstanceComponent(Mesh); Mesh->RegisterComponent();
	for (float Original : {1.0f, 0.4f})
	{
		for (int32 Case = 0; Case < 3; ++Case)
		{
			Settings->SetTimeDilation(Original * 0.15f);
			Magic->bImpactHolding = true;
			Magic->ImpactWorldSettings = Settings;
			Magic->PreviousTimeDilation = Original;
			Magic->ImpactEndsAt = 100.15;
			Magic->TargetActor = Target;
			Magic->Mesh = Mesh;
			TestTrue(TEXT("Real-time hold remains open"), Magic->UpdateCounterImpact(100.10));
			TestTrue(TEXT("No game-time-dependent early restore"), Magic->IsParryImpactActive());
			if (Case == 0) { Magic->StopPresentation(); }
			else if (Case == 1) { Magic->UpdateCounterImpact(100.16); }
			else { Magic->TargetActor.Reset(); Magic->UpdateCounterImpact(100.10); }
			TestFalse(TEXT("Hold always cleaned"), Magic->IsParryImpactActive());
			TestEqual(TEXT("Restore actual prior speed, including non-unit speed"), Settings->TimeDilation, Original);
			Magic->StopPresentation();
			TestEqual(TEXT("Repeated cleanup preserves speed"), Settings->TimeDilation, Original);
		}
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMagicDamageDataTest,
	"ExpeditionPortfolio.Combat.Magic.DamageData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMagicDamageDataTest::RunTest(const FString& Parameters)
{
	for (int32 Case = 0; Case < 6; ++Case)
	{
		FScopedBattleTestWorld TestWorld;
		auto* World = TestWorld.GetWorld(); auto* Battle = TestWorld.GetCoordinator();
		auto* Player = World->SpawnActor<APawn>(); auto* Enemy = World->SpawnActor<AActor>();
		AddCombatantComponent(Player); auto* HP = AddCombatantComponent(Enemy);
		auto* Magic = NewObject<UMagicPresentationComponent>(Player);
		Player->AddInstanceComponent(Magic); Magic->RegisterComponent();
		auto* Data = NewObject<UMagicActionData>(Player);
		Data->BaseDamage = Case == 2 ? 0.0f : Case == 3 ? -1.0f
			: Case == 4 ? std::numeric_limits<float>::quiet_NaN() : 17.0f;
		Magic->BasicMagic = Data; Magic->StrongMagic = Data;
		if (Case == 5) { Magic->BasicMagic = nullptr; }
		TestTrue(TEXT("Start"), Battle->RequestBattleStart(World->SpawnActor<AActor>(), Player, Enemy));
		const auto Command = Case == 1 ? EBattlePlayerCommand::PowerStrike : EBattlePlayerCommand::BasicAttack;
		const float Expected = Case == 2 ? 0.0f : Case >= 3 ? 25.0f : 17.0f;
		TestEqual(TEXT("UI query uses validated data"), Battle->GetMagicDamage(Command), Expected);
		Battle->OnBattleStateChanged().AddLambda([Data](EBattleState, EBattleState Next)
		{
			if (Next == EBattleState::PlayerAction) { Data->BaseDamage = 99.0f; }
		});
		TestTrue(TEXT("Command accepted"), Battle->RequestPlayerCommand(Command));
		TestEqual(TEXT("Impact uses command-start snapshot"), HP->GetCurrentHealth(), 100.0f - Expected);
		TestEqual(TEXT("Future command sees new tuning"), Battle->GetMagicDamage(Command), Case == 5 ? 25.0f : 99.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingRulesTest,
	"ExpeditionPortfolio.Combat.AttackTiming.Rules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAttackTimingRulesTest::RunTest(const FString& Parameters)
{
	FAttackTimingWindow Window;
	TestFalse(TEXT("Zero duration rejected"), Window.Begin(0,0,.3f,1.5f));
	TestFalse(TEXT("Window beyond preparation rejected"), Window.Begin(0,2.5f,3,1.5f));
	TestFalse(TEXT("Nonfinite multiplier rejected"), Window.Begin(0,2.5f,.3f,std::numeric_limits<float>::quiet_NaN()));
	TestTrue(TEXT("Start valid window"), Window.Begin(10,2.5f,.3f,1.5f));
	TestFalse(TEXT("Input before action rejected"), Window.Submit(9));
	TestTrue(TEXT("Early input consumes attempt"), Window.Submit(12.19));
	TestEqual(TEXT("White input is Regular"), Window.Result,EAttackTimingResult::Failed);
	TestFalse(TEXT("Cannot retry early failure"), Window.Submit(12.3));
	Window.Begin(10,2.5f,.3f,1.5f);
	TestTrue(TEXT("Opening boundary included"), Window.Submit(12.2));
	TestEqual(TEXT("Boundary succeeds"),Window.Result,EAttackTimingResult::Success);
	TestEqual(TEXT("Success multiplier"),Window.DamageMultiplier(),1.5f);
	Window.Close(true);
	TestEqual(TEXT("Cancellation removes uncommitted bonus"),Window.DamageMultiplier(),1.0f);
	Window.Begin(10,2.5f,.3f,1.5f);
	TestFalse(TEXT("Closing boundary excluded"),Window.Submit(12.5));
	Window.Close();
	TestEqual(TEXT("No input marked missed"),Window.Result,EAttackTimingResult::Missed);
	TestFalse(TEXT("Closed window rejects input"),Window.Submit(12.3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingIntegrationTest,
	"ExpeditionPortfolio.Combat.AttackTiming.ActionLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAttackTimingIntegrationTest::RunTest(const FString& Parameters)
{
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>();
		AddCombatantComponent(Player); auto* HP=AddCombatantComponent(Enemy);
		Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy); Battle->TryTransitionTo(EBattleState::PlayerAction);
		Battle->ActivePlayerActionId=101; Battle->ActivePlayerCommand=EBattlePlayerCommand::BasicAttack; Battle->PendingPlayerDamage=17;
		Battle->BeginPlayerPreparation(101,1,.2f,1.7f,true);
		TArray<FCombatNumber> Numbers;
		Battle->OnCombatNumber().AddLambda([&](const FCombatNumber& Number) { Numbers.Add(Number); });
		World->GetSubsystem<UActionQTESubsystem>()->Timing.StartedAt-=.9;
		TestTrue(TEXT("QTE-enabled non-strong action accepts input"),Battle->RequestAttackTimingInput());
		Battle->NotifyPlayerPreparationFinished(101); Battle->NotifyPlayerActionHit(101);
		TestTrue(TEXT("Generic QTE bonus applies without PowerStrike check"),FMath::IsNearlyEqual(HP->GetCurrentHealth(),71.1f));
		TestEqual(TEXT("QTE impact has one number"),Numbers.Num(),1);
		if (Numbers.Num()==1)
		{
			TestEqual(TEXT("QTE multiplier marks bonus color"),Numbers[0].Kind,ECombatNumberKind::BonusDamage);
			TestTrue(TEXT("Bonus number matches actual loss"),FMath::IsNearlyEqual(Numbers[0].Amount,28.9f,.001f));
		}
		TestFalse(TEXT("Duplicate impact has no second number"),Battle->NotifyPlayerActionHit(101));
		TestEqual(TEXT("Number count after duplicate"),Numbers.Num(),1);
	}
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>(); auto* Encounter=World->SpawnActor<AActor>();
		AddCombatantComponent(Player); auto* HP=AddCombatantComponent(Enemy);
		Battle->RequestBattleStart(Encounter,Player,Enemy); Battle->TryTransitionTo(EBattleState::PlayerAction);
		Battle->ActivePlayerActionId=81; Battle->ActivePlayerCommand=EBattlePlayerCommand::PowerStrike; Battle->PendingPlayerDamage=40;
		Battle->BeginPlayerPreparation(81,1,.2f,1.5f,true); World->GetSubsystem<UActionQTESubsystem>()->Timing.StartedAt-=.9;
		TestTrue(TEXT("Timing success accepted"),Battle->RequestAttackTimingInput());
		AdvanceBattleTime(World,1.01f);
		TestFalse(TEXT("Gauge closes independently of preparation"),Battle->GetAttackTimingView().bActive);
		TestEqual(TEXT("Success survives post-gauge hold"),Battle->GetAttackTimingView().Result,EAttackTimingResult::Success);
		TestEqual(TEXT("Closing gauge does not launch attack"),Battle->GetPlayerActionPhase(),EPlayerActionPhase::Preparing);
		TestFalse(TEXT("Hold rejects another timing input"),Battle->RequestAttackTimingInput());
		TestFalse(TEXT("Hold rejects premature hit"),Battle->NotifyPlayerActionHit(81));
		TestEqual(TEXT("Hold preserves HP"),HP->GetCurrentHealth(),100.f);
		Battle->NotifyPlayerPreparationFinished(81);
		TestTrue(TEXT("Release accepts hit"),Battle->NotifyPlayerActionHit(81));
		TestEqual(TEXT("Bonus remains after hold"),HP->GetCurrentHealth(),40.f);
	}
	for (int32 Case=0; Case<5; ++Case)
	{
		FScopedBattleTestWorld TestWorld;
		auto* World=TestWorld.GetWorld(); auto* Battle=TestWorld.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>(); auto* Encounter=World->SpawnActor<AActor>();
		AddCombatantComponent(Player); auto* HP=AddCombatantComponent(Enemy);
		TestTrue(TEXT("Start"),Battle->RequestBattleStart(Encounter,Player,Enemy));
		Battle->TryTransitionTo(EBattleState::PlayerAction);
		Battle->ActivePlayerActionId=31;
		Battle->ActivePlayerCommand=EBattlePlayerCommand::PowerStrike;
		Battle->PendingPlayerDamage=40;
		Battle->BeginPlayerPreparation(31,2.5f,.3f,1.5f,true);
		TestTrue(TEXT("View exposes timing window"),Battle->GetAttackTimingView().bActive);
		Battle->NotifyPlayerPreparationFinished(30);
		TestEqual(TEXT("Stale completion ignored"),Battle->GetPlayerActionPhase(),EPlayerActionPhase::Preparing);
		if (Case!=2)
		{
			if (Case!=1) { World->GetSubsystem<UActionQTESubsystem>()->Timing.StartedAt-=2.3; }
			bool Reentered=true;
			auto Handle=Battle->OnAttackTimingChanged().AddLambda([&]{ Reentered=Battle->RequestAttackTimingInput(); });
			TestTrue(TEXT("First input accepted"),Battle->RequestAttackTimingInput());
			TestFalse(TEXT("Result event reentry rejected"),Reentered);
			Battle->OnAttackTimingChanged().Remove(Handle);
			TestFalse(TEXT("Repeated input rejected"),Battle->RequestAttackTimingInput());
		}
		TestFalse(TEXT("Bonus input cannot apply damage during preparation"),Battle->NotifyPlayerActionHit(31));
		TestEqual(TEXT("Preparation leaves HP untouched"),HP->GetCurrentHealth(),100.0f);
		if (Case==3 || Case==4)
		{
			if (Case==4)
			{
				AddExpectedError(TEXT("completion timed out"),EAutomationExpectedErrorFlags::Contains,1);
				Battle->HandlePlayerActionTimeout(31);
			}
			else { Battle->NotifyPlayerActionFinished(31,true); }
			TestEqual(TEXT("Interrupted success never invents a hit"),HP->GetCurrentHealth(),100.0f);
			TestFalse(TEXT("Interrupted window closed"),Battle->GetAttackTimingView().bActive);
		}
		else
		{
			Battle->NotifyPlayerPreparationFinished(31);
			TestTrue(TEXT("Attack hit accepted"),Battle->NotifyPlayerActionHit(31));
			TestEqual(TEXT("Only successful action receives bonus"),HP->GetCurrentHealth(),Case==0 ? 40.0f : 60.0f);
			TestFalse(TEXT("No duplicate damage"),Battle->NotifyPlayerActionHit(31));
			Battle->NotifyPlayerActionFinished(31,false);
		}
		TestFalse(TEXT("Next phase rejects old timing input"),Battle->RequestAttackTimingInput());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleFacingAndCameraTest,
	"ExpeditionPortfolio.Combat.Presentation.FacingAndCamera", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBattleFacingAndCameraTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld TestWorld; auto* World=TestWorld.GetWorld();
	auto* Player=World->SpawnActor<ACharacter>(); auto* Enemy=World->SpawnActor<ACharacter>();
	Player->SetActorLocation(FVector(100,30,100)); Enemy->SetActorLocation(FVector(-150,380,150));
	const bool OldOrient=Player->GetCharacterMovement()->bOrientRotationToMovement;
	FBattleFacingRestore Restore; Restore.Capture(Player); Restore.Face(Player,Enemy); Restore.Face(Enemy,Player);
	const FVector Direction=(Enemy->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D();
	TestTrue(TEXT("Player faces enemy"),FVector::DotProduct(Player->GetActorForwardVector(),Direction)>.999);
	TestTrue(TEXT("Enemy faces player"),FVector::DotProduct(Enemy->GetActorForwardVector(),-Direction)>.999);
	TestEqual(TEXT("Facing has no pitch"),Player->GetActorRotation().Pitch,0.0);
	TestFalse(TEXT("Movement cannot overwrite combat rotation"),Player->GetCharacterMovement()->bOrientRotationToMovement);
	Restore.Restore();
	TestEqual(TEXT("Movement rotation restored"),Player->GetCharacterMovement()->bOrientRotationToMovement,OldOrient);
	auto* Camera=World->SpawnActor<ABattleCameraRig>(); FBattleCameraShot Shot;
	Camera->ConfigureShot(Shot,Player,Enemy);
	FVector Aim=FMath::Lerp(Player->GetActorLocation(),Enemy->GetActorLocation(),double(Shot.LookAtEnemyWeight))+FVector(0,0,Shot.LookHeight);
	TestTrue(TEXT("Camera looks at configured battle target"),FVector::DotProduct(Camera->GetActorForwardVector(),(Aim-Camera->GetActorLocation()).GetSafeNormal())>.999);
	TestEqual(TEXT("FOV comes from data"),Camera->GetCameraComponent()->FieldOfView,Shot.FieldOfView);
	const FVector Before=Player->GetActorLocation();
	Camera->ConfigureShot(Shot,Player,Enemy);
	TestTrue(TEXT("Camera does not move combatant"),Player->GetActorLocation().Equals(Before));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetSelectionLifecycleTest,
	"ExpeditionPortfolio.Combat.TargetSelection.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTargetSelectionLifecycleTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope; auto* World = Scope.GetWorld(); auto* Battle = Scope.GetCoordinator();
	auto* Player = World->SpawnActor<APawn>(); auto* Enemy = World->SpawnActor<AActor>(); auto* Encounter = World->SpawnActor<AActor>();
	AddCombatantComponent(Player); auto* Health = AddCombatantComponent(Enemy);
	TestTrue(TEXT("Start encounter"), Battle->RequestBattleStart(Encounter, Player, Enemy));
	bool Reentered = true;
	auto Handle = Battle->OnTargetSelectionChanged().AddLambda([&] { Reentered = Battle->ConfirmTargetSelection(); });
	TestTrue(TEXT("Strong magic enters selection"), Battle->BeginCommandSelection(EBattlePlayerCommand::PowerStrike));
	TestFalse(TEXT("Selection notification rejects reentry"), Reentered);
	TestEqual(TEXT("Selection stays in command turn"), Battle->GetCurrentState(), EBattleState::PlayerCommand);
	TestEqual(TEXT("No damage before confirmation"), Health->GetCurrentHealth(), 100.f);
	TestTrue(TEXT("Single enemy still selected"), Battle->GetSelectedTarget() == Enemy);
	TestTrue(TEXT("Right with one target is safe"), Battle->MoveTargetSelection(1));
	TestTrue(TEXT("Left with one target is safe"), Battle->MoveTargetSelection(-1));
	TestFalse(TEXT("Cannot replace pending command"), Battle->BeginCommandSelection(EBattlePlayerCommand::BasicAttack));
	TestFalse(TEXT("Atomic API cannot bypass pending selection"), Battle->RequestPlayerBasicAttack());
	TestTrue(TEXT("Cancel"), Battle->CancelTargetSelection());
	TestFalse(TEXT("Cancel clears target"), Battle->IsSelectingTarget());
	TestTrue(TEXT("Cancel preserves strong charge"), Battle->CanRequestPlayerCommand(EBattlePlayerCommand::PowerStrike));
	Battle->OnTargetSelectionChanged().Remove(Handle);
	TestTrue(TEXT("Select again"), Battle->BeginCommandSelection(EBattlePlayerCommand::PowerStrike));
	TestTrue(TEXT("Confirm executes"), Battle->ConfirmTargetSelection());
	TestTrue(TEXT("Camera return is pending"), Battle->IsConfirmingTarget());
	TestEqual(TEXT("No damage at confirmation"), Health->GetCurrentHealth(), 100.f);
	TestFalse(TEXT("Cannot issue another command during camera return"), Battle->BeginCommandSelection(EBattlePlayerCommand::BasicAttack));
	TestFalse(TEXT("Cannot bypass camera return"), Battle->RequestPlayerBasicAttack());
	TestFalse(TEXT("Cannot cancel committed command"), Battle->CancelTargetSelection());
	TestFalse(TEXT("Cannot confirm twice during camera return"), Battle->ConfirmTargetSelection());
	AdvanceBattleTime(World, 0.2f);
	TestEqual(TEXT("No damage before camera return finishes"), Health->GetCurrentHealth(), 100.f);
	AdvanceBattleTime(World, 0.3f);
	TestFalse(TEXT("Camera return flag cleared"), Battle->IsConfirmingTarget());
	TestEqual(TEXT("Headless attack applies one damage amount"), Health->GetCurrentHealth(), 60.f);
	TestFalse(TEXT("Repeated confirm rejected"), Battle->ConfirmTargetSelection());
	CompleteNoInputEnemyTurn(World);
	TestFalse(TEXT("Confirmed strong charge consumed"), Battle->CanRequestPlayerCommand(EBattlePlayerCommand::PowerStrike));
	TestTrue(TEXT("Select basic"), Battle->BeginCommandSelection(EBattlePlayerCommand::BasicAttack));
	Enemy->Destroy();
	TestFalse(TEXT("Destroyed target cannot be confirmed"), Battle->ConfirmTargetSelection());
	TestEqual(TEXT("Invalid battle returns to field"), Battle->GetCurrentState(), EBattleState::Exploration);
	TestFalse(TEXT("Invalid battle clears selection"), Battle->IsSelectingTarget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetConfirmationInvalidationTest,
	"ExpeditionPortfolio.Combat.TargetSelection.ConfirmationInvalidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTargetConfirmationInvalidationTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope; auto* World = Scope.GetWorld(); auto* Battle = Scope.GetCoordinator();
	auto* Player = World->SpawnActor<APawn>(); auto* Enemy = World->SpawnActor<AActor>(); auto* Encounter = World->SpawnActor<AActor>();
	AddCombatantComponent(Player); auto* Health = AddCombatantComponent(Enemy);
	Battle->RequestBattleStart(Encounter, Player, Enemy);
	Battle->BeginCommandSelection(EBattlePlayerCommand::PowerStrike);
	TestTrue(TEXT("Confirmation starts camera return"), Battle->ConfirmTargetSelection());
	Health->ApplyDamage(100);
	AdvanceBattleTime(World, .5f);
	TestEqual(TEXT("Invalid target during return safely exits battle"), Battle->GetCurrentState(), EBattleState::Exploration);
	TestFalse(TEXT("Pending confirmation cleared"), Battle->IsConfirmingTarget());
	Enemy = World->SpawnActor<AActor>();
	Health = AddCombatantComponent(Enemy);
	TestTrue(TEXT("New battle may start"), Battle->RequestBattleStart(Encounter, Player, Enemy));
	AdvanceBattleTime(World, 1.f);
	TestEqual(TEXT("Old confirmation cannot attack in new battle"), Health->GetCurrentHealth(), 100.f);
	TestEqual(TEXT("New command phase undisturbed"), Battle->GetCurrentState(), EBattleState::PlayerCommand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionQTESessionsTest,
	"ExpeditionPortfolio.Combat.QTE.Sessions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FActionQTESessionsTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* QTE=World->GetSubsystem<UActionQTESubsystem>();
	int32 Starts=0, Results=0, Cancels=0; int64 Reentry=-1;
	QTE->Started.AddLambda([&](int64) { ++Starts; Reentry=QTE->BeginQTE(1,.2f); });
	QTE->Resolved.AddLambda([&](int64 Id, EAttackTimingResult) { ++Results; TestFalse(TEXT("Result callback cannot resubmit"),QTE->SubmitInput(Id)); });
	QTE->Cancelled.AddLambda([&](int64) { ++Cancels; });
	TestEqual(TEXT("Invalid settings emit nothing"),QTE->BeginQTE(0,.2f),int64(0));
	const int64 Regular=QTE->BeginQTE(1,.2f);
	TestTrue(TEXT("Session created"),Regular>0);
	TestEqual(TEXT("Start reentry blocked"),Reentry,int64(0));
	TestEqual(TEXT("Active session cannot be overwritten"),QTE->BeginQTE(1,.2f),int64(0));
	TestTrue(TEXT("White input accepted"),QTE->SubmitInput(Regular));
	TestEqual(TEXT("White grade is Regular"),QTE->GetView().Result,EAttackTimingResult::Failed);
	TestFalse(TEXT("No repeated input"),QTE->SubmitInput(Regular));
	AdvanceBattleTime(World,1.01f);
	TestEqual(TEXT("Closing Regular does not emit Miss"),Results,1);
	const int64 Perfect=QTE->BeginQTE(1,.2f,1.8f);
	TestTrue(TEXT("Monotonic session tokens"),Perfect>Regular);
	TestFalse(TEXT("Stale input rejected"),QTE->SubmitInput(Regular));
	QTE->Timing.StartedAt-=.9;
	QTE->SubmitInput(Perfect); QTE->FinishWindow(Perfect);
	TestEqual(TEXT("Perfect grade"),QTE->GetView().Result,EAttackTimingResult::Success);
	TestEqual(TEXT("Per-session multiplier"),QTE->GetDamageMultiplier(Perfect),1.8f);
	TestEqual(TEXT("Old session cannot lend bonus"),QTE->GetDamageMultiplier(Regular),1.f);
	const int64 Miss=QTE->BeginQTE(.1f,.02f);
	AdvanceBattleTime(World,.11f);
	TestEqual(TEXT("Timeout resolves Miss without a widget"),QTE->GetView().Result,EAttackTimingResult::Missed);
	TestEqual(TEXT("Only one result each"),Results,3);
	const int64 Cancel=QTE->BeginQTE(1,.2f);
	QTE->CancelQTE(Miss); TestTrue(TEXT("Stale cancel cannot close new QTE"),QTE->GetView().bActive);
	QTE->CancelQTE(Cancel); AdvanceBattleTime(World,1.01f);
	TestEqual(TEXT("Cancel has no Miss result"),Results,3);
	TestEqual(TEXT("Cancellation event once"),Cancels,1);
	TestEqual(TEXT("Start event once per session"),Starts,4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetSelectionOrderingTest,
	"ExpeditionPortfolio.Combat.TargetSelection.Ordering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTargetSelectionOrderingTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope; auto* World = Scope.GetWorld();
	auto* Left = World->SpawnActor<ACharacter>(); auto* Middle = World->SpawnActor<ACharacter>(); auto* Right = World->SpawnActor<ACharacter>();
	AddCombatantComponent(Left); auto* MiddleHP = AddCombatantComponent(Middle); AddCombatantComponent(Right);
	Left->SetActorLocation(FVector(0,-300,100)); Middle->SetActorLocation(FVector(0,0,100)); Right->SetActorLocation(FVector(0,300,100));
	FCombatTargetSelection Selection;
	Selection.Build({Right,Left,Middle,Left}, FVector::RightVector);
	TestEqual(TEXT("Unique candidates"), Selection.Candidates.Num(), 3);
	TestTrue(TEXT("Order follows captured screen-right axis"), Selection.Get() == Left);
	Selection.Move(-1); TestTrue(TEXT("Left wraps"), Selection.Get() == Right);
	Selection.Move(1); TestTrue(TEXT("Right wraps"), Selection.Get() == Left);
	Selection.Move(1); TestTrue(TEXT("Next candidate"), Selection.Get() == Middle);
	MiddleHP->ApplyDamage(100); Selection.Move(0);
	TestTrue(TEXT("Dead target replaced"), Selection.Get() == Right);
	Right->Destroy(); Selection.Move(1); TestTrue(TEXT("Destroyed target pruned"), Selection.Get() == Left);
	Left->Destroy(); Selection.Move(-1); TestNull(TEXT("No valid candidates"), Selection.Get());
	auto* Player = World->SpawnActor<ACharacter>(); auto* Enemy = World->SpawnActor<ACharacter>();
	Player->SetActorLocation(FVector(-1000,0,100)); Enemy->SetActorLocation(FVector(500,0,100));
	auto* Rig = World->SpawnActor<ABattleCameraRig>(); UBattlePresentationData* Data = NewObject<UBattlePresentationData>();
	Rig->ConfigureShot(Data->TargetSelectionShot, Player, Enemy);
	TestTrue(TEXT("Target shot uses enemy origin"), Rig->GetActorLocation().Equals(Enemy->GetActorLocation()+Data->TargetSelectionShot.Offset));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemAllyTargetTest,
	"ExpeditionPortfolio.Combat.TargetSelection.ItemAllies",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FItemAllyTargetTest::RunTest(const FString& Parameters)
{
	for(int32 Case=0;Case<3;++Case)
	{
		FScopedBattleTestWorld Scope;auto* World=Scope.GetWorld();auto* Battle=Scope.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>();auto* Enemy=World->SpawnActor<AActor>();auto* Encounter=World->SpawnActor<AActor>();auto* Ally=World->SpawnActor<AActor>();
		auto* PlayerHP=AddCombatantComponent(Player);AddCombatantComponent(Enemy);auto* AllyHP=AddCombatantComponent(Ally);AllyHP->ApplyDamage(60);
		Battle->RequestBattleStart(Encounter,Player,Enemy);
		TestFalse(TEXT("Enemy cannot register as ally"),Battle->RegisterBattleAlly(Enemy));
		TestTrue(TEXT("Future ally can register"),Battle->RegisterBattleAlly(Ally));
		const int32 Count=Battle->GetRemainingPotions();
		TestTrue(TEXT("Full health caster can heal wounded ally"),Battle->BeginCommandSelection(EBattlePlayerCommand::UsePotion));
		TestTrue(TEXT("Full health self remains selectable"),Battle->IsSelectingAlly()&&Battle->GetSelectedTarget()==Player);
		TestEqual(TEXT("Both full and wounded allies are candidates"),Battle->GetTargetCount(),2);
		TestEqual(TEXT("Selection does not consume item"),Battle->GetRemainingPotions(),Count);
		int32 Moves=0;Battle->OnTargetNavigation().AddLambda([&](){++Moves;});
		Battle->MoveTargetSelection(1);TestEqual(TEXT("Navigation signals once"),Moves,1);
		TestTrue(TEXT("Navigation selects wounded ally"),Battle->GetSelectedTarget()==Ally);
		if(Case==0)
		{
			Battle->CancelTargetSelection();TestEqual(TEXT("Cancel preserves potion"),Battle->GetRemainingPotions(),Count);
			TestEqual(TEXT("Cancel does not heal"),AllyHP->GetCurrentHealth(),40.f);continue;
		}
		TestTrue(TEXT("Item requires confirmation"),Battle->ConfirmTargetSelection());
		TestFalse(TEXT("Repeated confirm rejected"),Battle->ConfirmTargetSelection());
		TestEqual(TEXT("Camera wait preserves potion"),Battle->GetRemainingPotions(),Count);
		if(Case==2){Ally->Destroy();}
		AdvanceBattleTime(World,.5f);
		TestEqual(TEXT("Caster never receives ally healing"),PlayerHP->GetCurrentHealth(),100.f);
		if(Case==1)
		{
			TestEqual(TEXT("Selected ally healed once"),AllyHP->GetCurrentHealth(),70.f);
			TestEqual(TEXT("Exactly one potion consumed"),Battle->GetRemainingPotions(),Count-1);
		}
		else{TestEqual(TEXT("Invalidated confirmation consumes nothing"),Battle->GetRemainingPotions(),Count);}
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullHealthPotionTest,
	"ExpeditionPortfolio.Combat.Items.FullHealthPotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullHealthPotionTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope;auto* World=Scope.GetWorld();auto* Battle=Scope.GetCoordinator();
	FBattleCoordinatorTestAccess::SetTurnDelay(Battle,1);
	auto* Player=World->SpawnActor<APawn>();auto* Enemy=World->SpawnActor<AActor>();auto* Encounter=World->SpawnActor<AActor>();
	auto* HP=AddCombatantComponent(Player);AddCombatantComponent(Enemy);
	Battle->RequestBattleStart(Encounter,Player,Enemy);
	int32 Changes=0;HP->OnHealthChanged().AddLambda([&](float,float){++Changes;});
	TestTrue(TEXT("Full HP target selection accepted"),Battle->BeginCommandSelection(EBattlePlayerCommand::UsePotion));
	TestTrue(TEXT("Cancel full HP item"),Battle->CancelTargetSelection());
	TestEqual(TEXT("Cancel keeps stock"),Battle->GetRemainingPotions(),3);
	TestTrue(TEXT("Select again"),Battle->BeginCommandSelection(EBattlePlayerCommand::UsePotion));
	TestTrue(TEXT("Full HP confirmation accepted"),Battle->ConfirmTargetSelection());
	TestFalse(TEXT("Duplicate confirmation rejected"),Battle->ConfirmTargetSelection());
	AdvanceBattleTime(World,.5f);
	TestEqual(TEXT("Full HP remains unchanged"),HP->GetCurrentHealth(),100.f);
	TestEqual(TEXT("No fake HP change event"),Changes,0);
	TestEqual(TEXT("Consumes exactly one potion"),Battle->GetRemainingPotions(),2);
	TestEqual(TEXT("Full HP item consumes turn"),Battle->GetCurrentState(),EBattleState::TurnEnd);
	TestFalse(TEXT("Cannot repeat during turn gap"),Battle->RequestPlayerCommand(EBattlePlayerCommand::UsePotion));
	AdvanceBattleTime(World,1.01f);
	TestEqual(TEXT("Continues into enemy turn"),Battle->GetCurrentState(),EBattleState::EnemyTelegraph);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnTransitionDelayTest,
	"ExpeditionPortfolio.Combat.Turn.TransitionDelay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurnTransitionDelayTest::RunTest(const FString& Parameters)
{
	for(int32 Case=0;Case<3;++Case)
	{
		FScopedBattleTestWorld Scope;auto* World=Scope.GetWorld();auto* Battle=Scope.GetCoordinator();
		FBattleCoordinatorTestAccess::SetTurnDelay(Battle,1);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleResultSnapshotTest,
	"ExpeditionPortfolio.Combat.Result.SnapshotAndOverkill",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBattleResultSnapshotTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
	auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>(); auto* Ally=World->SpawnActor<AActor>();
	auto* PlayerHP=AddCombatantComponent(Player); auto* EnemyHP=AddCombatantComponent(Enemy); auto* AllyHP=AddCombatantComponent(Ally);
	PlayerHP->ApplyDamage(35); EnemyHP->ApplyDamage(93); AllyHP->ApplyDamage(20);
	Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy); Battle->RegisterBattleAlly(Ally);
	TestTrue(TEXT("Lethal command accepted"),Battle->RequestPlayerBasicAttack());
	TestEqual(TEXT("Result waits for explicit confirmation"),Battle->GetCurrentState(),EBattleState::Victory);
	TestEqual(TEXT("Overkill records only lost HP"),Battle->GetBattleResult().DamageDealt,7.f);
	TestEqual(TEXT("Pre-battle damage excluded"),Battle->GetBattleResult().DamageReceived,0.f);
	TestEqual(TEXT("Registered party rows"),Battle->GetBattleResult().Party.Num(),2);
	TestEqual(TEXT("Remaining HP snapshot"),Battle->GetBattleResult().Party[0].Health,65.f);
	TestFalse(TEXT("Commands locked on results"),Battle->RequestPlayerBasicAttack());
	AdvanceBattleTime(World,20);
	TestEqual(TEXT("No delayed opponent turn"),Battle->GetCurrentState(),EBattleState::Victory);
	TestTrue(TEXT("Continue"),Battle->ContinueAfterVictory());
	TestFalse(TEXT("Repeated result confirmation rejected"),Battle->ContinueAfterVictory());
	auto* NextEnemy=World->SpawnActor<AActor>(); AddCombatantComponent(NextEnemy);
	TestTrue(TEXT("Next encounter"),Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,NextEnemy));
	TestEqual(TEXT("Next battle preserves HP"),PlayerHP->GetCurrentHealth(),65.f);
	TestEqual(TEXT("Next battle clears stats"),Battle->GetBattleResult().DamageDealt,0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleResultRetryTest,
	"ExpeditionPortfolio.Combat.Result.RetryEntrySnapshot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBattleResultRetryTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
	auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>(); auto* Ally=World->SpawnActor<AActor>();
	auto* PlayerHP=AddCombatantComponent(Player); auto* EnemyHP=AddCombatantComponent(Enemy); auto* AllyHP=AddCombatantComponent(Ally);
	PlayerHP->ApplyDamage(80); AllyHP->ApplyDamage(60);
	const int32 Repeats=3;
	Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy); Battle->RegisterBattleAlly(Ally);
	for (int32 I=0;I<Repeats;++I)
	{
		TestTrue(TEXT("Consumes strong skill"),Battle->RequestPlayerPowerStrike());
		CompleteNoInputEnemyTurn(World);
		TestEqual(TEXT("Defeat on lethal enemy hit"),Battle->GetCurrentState(),EBattleState::Defeat);
		TestEqual(TEXT("Damage received limited by current HP"),Battle->GetBattleResult().DamageReceived,20.f);
		TestFalse(TEXT("Cannot continue defeat as victory"),Battle->ContinueAfterVictory());
		TestTrue(TEXT("Retry"),Battle->RetryBattle());
		TestFalse(TEXT("Retry preserves enemy actor"),Enemy->IsActorBeingDestroyed());
		TestEqual(TEXT("Retry restores entry HP, not max"),PlayerHP->GetCurrentHealth(),20.f);
		TestEqual(TEXT("Enemy HP restored"),EnemyHP->GetCurrentHealth(),100.f);
		TestEqual(TEXT("Other party HP restored"),AllyHP->GetCurrentHealth(),40.f);
		TestTrue(TEXT("Strong skill restored"),Battle->CanRequestPlayerCommand(EBattlePlayerCommand::PowerStrike));
		TestEqual(TEXT("Potion supply restored"),Battle->GetRemainingPotions(),3);
		TestEqual(TEXT("Retry resets stats"),Battle->GetBattleResult().DamageReceived,0.f);
		TestFalse(TEXT("Repeated retry while fighting rejected"),Battle->RetryBattle());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyHealthSessionTest,
	"ExpeditionPortfolio.Combat.Result.PersistentHealthStore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPartyHealthSessionTest::RunTest(const FString& Parameters)
{
	auto* Instance=NewObject<UGameInstance>(); auto* Store=NewObject<UPartyHealthSubsystem>(Instance);
	float Health=0;
	TestFalse(TEXT("New character has no saved state"),Store->FindHealth(TEXT("Player"),Health));
	Store->StoreHealth(TEXT("Player"),37); Store->StoreHealth(TEXT("Ally"),81);
	TestTrue(TEXT("Stable ID lookup"),Store->FindHealth(TEXT("Player"),Health)); TestEqual(TEXT("Player snapshot"),Health,37.f);
	TestTrue(TEXT("Independent member"),Store->FindHealth(TEXT("Ally"),Health)); TestEqual(TEXT("Ally snapshot"),Health,81.f);
	Store->StoreHealth(TEXT("Player"),0); Store->FindHealth(TEXT("Player"),Health); TestEqual(TEXT("Zero HP persists"),Health,0.f);
	Store->StoreHealth(NAME_None,99); TestFalse(TEXT("Transient enemies excluded"),Store->FindHealth(NAME_None,Health));
	Store->StoreHealth(TEXT("Player"),std::numeric_limits<float>::quiet_NaN()); Store->FindHealth(TEXT("Player"),Health); TestEqual(TEXT("Invalid update rejected"),Health,0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleResultUnsafeRetreatTest,
	"ExpeditionPortfolio.Combat.Result.UnsafeRetreatKeepsResult",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBattleResultUnsafeRetreatTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
	auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>();
	auto* PlayerHP=AddCombatantComponent(Player); AddCombatantComponent(Enemy);
	PlayerHP->ApplyDamage(80);
	// This isolated world deliberately has no floor or safe retreat point.
	auto* Trigger=World->SpawnActor<ABattleEncounterTrigger>();
	TestTrue(TEXT("Start encounter"),Battle->RequestBattleStart(Trigger,Player,Enemy));
	Battle->RequestPlayerBasicAttack(); CompleteNoInputEnemyTurn(World);
	TestEqual(TEXT("Defeated"),Battle->GetCurrentState(),EBattleState::Defeat);
	AddExpectedError(TEXT("No safe retreat location"),EAutomationExpectedErrorFlags::Contains,1);
	TestFalse(TEXT("Unsafe teleport rejected"),Battle->RetreatFromBattle());
	TestEqual(TEXT("Failed retreat keeps result"),Battle->GetCurrentState(),EBattleState::Defeat);
	TestEqual(TEXT("Failed retreat does not partially restore HP"),PlayerHP->GetCurrentHealth(),0.f);
	TestTrue(TEXT("Retry remains possible"),Battle->RetryBattle());
	TestEqual(TEXT("Entry HP restored"),PlayerHP->GetCurrentHealth(),20.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionQTEPatternsTest,
	"ExpeditionPortfolio.Combat.QTE.Patterns",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FActionQTEPatternsTest::RunTest(const FString& Parameters)
{
	for (int32 Mode=0; Mode<2; ++Mode)
	for (int32 A=0; A<3; ++A)
	for (int32 B=0; B<3; ++B)
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* QTE=World->GetSubsystem<UActionQTESubsystem>();
		auto* Pattern=NewObject<UActionQTEPatternData>();
		FActionQTEStep Step; Step.Duration=1; Step.PerfectDuration=.2f; Step.DelayAfter=Mode==0?.15f:0;
		Pattern->Steps={Step,Step}; Pattern->Mode=Mode==0?EQTEPatternMode::Sequential:EQTEPatternMode::Rhythm;
		Pattern->SpawnInterval=.6f;
		int32 StepResults=0,FinalResults=0,Starts=0;
		QTE->StepStarted.AddLambda([&](int64,int32) { ++Starts; });
		QTE->StepResolved.AddLambda([&](int64 Id,int32,EAttackTimingResult) { ++StepResults; TestFalse(TEXT("Step callback input reentry blocked"),QTE->SubmitInput(Id)); });
		QTE->Resolved.AddLambda([&](int64,EAttackTimingResult) { ++FinalResults; });
		const int64 Session=QTE->BeginPattern(Pattern);
		TestTrue(TEXT("Two-step pattern starts"),Session>0);
		Pattern->Steps[0].Duration=9; Pattern->PerfectMultiplier=9;
		const int32 Grades[]={A,B};
		if (Mode==1)
		{
			AdvanceBattleTime(World,.65f);
			TestEqual(TEXT("Two independent gauges coexist before first expiry"),QTE->GetView().Gauges.Num(),2);
			TestTrue(TEXT("Independent gauge progress differs"),QTE->GetView().Gauges[0].Progress>QTE->GetView().Gauges[1].Progress);
			for (int32 I=0; I<2; ++I)
			{
				if (Grades[I]==2) { continue; }
				// First Regular is still possible at .65; second Regular at 1.1, Perfect at 1.5.
				const double At=I*.6+(Grades[I]==0?.9:(I==0?.65:.5));
				AdvanceBattleTime(World,FMath::Max(0.f,float(At-World->GetTimeSeconds())));
				TestTrue(TEXT("Overlap accepts one chronological gauge"),QTE->SubmitInput(Session));
				TestFalse(TEXT("Same-time duplicate cannot consume another overlapping gauge"),QTE->SubmitInput(Session));
			}
			AdvanceBattleTime(World,FMath::Max(0.f,float(1.65-World->GetTimeSeconds())));
			const auto Expected=(A==2||B==2)?EAttackTimingResult::Missed:(A==0&&B==0)?EAttackTimingResult::Success:EAttackTimingResult::Failed;
			TestEqual(TEXT("Any overlapping Miss fails whole pattern"),QTE->GetView().OverallResult,Expected);
			TestEqual(TEXT("Overlap aggregate multiplier"),QTE->GetDamageMultiplier(Session),(A==2||B==2)?1.f:1.f+.25f*((A==0)+(B==0)));
			TestFalse(TEXT("Overlap closes after last independent window"),QTE->GetView().bActive);
			TestEqual(TEXT("Overlap step result events once"),StepResults,2);
			TestEqual(TEXT("Overlap aggregate event once"),FinalResults,1);
			TestEqual(TEXT("Overlap spawn events once"),Starts,2);
			continue;
		}
		for (int32 I=0; I<2; ++I)
		{
			if (QTE->Timing.StartedAt>World->GetTimeSeconds())
			{
				TestFalse(TEXT("Gap input rejected"),QTE->SubmitInput(Session));
				AdvanceBattleTime(World,float(QTE->Timing.StartedAt-World->GetTimeSeconds())+.001f);
			}
			TestEqual(TEXT("Ordered current step"),QTE->GetView().StepIndex,I);
			const double Start=QTE->Timing.StartedAt;
			const double InputTime=Start+(Grades[I]==0?.9:Grades[I]==1?.1:1.01);
			AdvanceBattleTime(World,FMath::Max(0.f,float(InputTime-World->GetTimeSeconds())));
			if (Grades[I]!=2)
			{
				TestTrue(TEXT("One input accepted for this step"),QTE->SubmitInput(Session));
				TestFalse(TEXT("Repeated press cannot skip next step"),QTE->SubmitInput(Session));
				AdvanceBattleTime(World,float(Start+1.01-World->GetTimeSeconds()));
			}
		}
		const auto Expected=(A==2||B==2)?EAttackTimingResult::Missed:(A==0&&B==0)?EAttackTimingResult::Success:EAttackTimingResult::Failed;
		TestEqual(TEXT("Any Miss fails the whole pattern"),QTE->GetView().OverallResult,Expected);
		const float Multiplier=(A==2||B==2)?1.f:1.f+.25f*((A==0)+(B==0));
		TestEqual(TEXT("Miss removes every bonus; successful steps average"),QTE->GetDamageMultiplier(Session),Multiplier);
		TestFalse(TEXT("Pattern closes after last window"),QTE->GetView().bActive);
		TestEqual(TEXT("One result per step"),StepResults,2);
		TestEqual(TEXT("One aggregate event"),FinalResults,1);
		TestEqual(TEXT("One start per step including gap"),Starts,2);
	}
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* QTE=World->GetSubsystem<UActionQTESubsystem>();
		auto* Pattern=NewObject<UActionQTEPatternData>();
		TestEqual(TEXT("Empty pattern rejected"),QTE->BeginPattern(Pattern),int64(0));
		FActionQTEStep Step; Step.Duration=.2f; Step.PerfectDuration=.05f;
		Pattern->Steps={Step,Step,Step,Step};
		const int64 Session=QTE->BeginPattern(Pattern);
		AdvanceBattleTime(World,.85f);
		TestEqual(TEXT("Hitch expires all overdue steps"),QTE->GetView().StepIndex,3);
		TestEqual(TEXT("Hitch cannot invent success"),QTE->GetView().OverallResult,EAttackTimingResult::Missed);
		const int64 NewSession=QTE->BeginPattern(Pattern);
		QTE->CancelQTE(Session); TestTrue(TEXT("Stale cancel cannot affect new pattern"),QTE->GetView().bActive);
		QTE->CancelQTE(NewSession); AdvanceBattleTime(World,1);
		TestEqual(TEXT("Cancel cannot turn into timeout Miss"),QTE->GetView().Result,EAttackTimingResult::Cancelled);
		Pattern->Steps[0].PerfectDuration=3;
		TestEqual(TEXT("Invalid step rejected"),QTE->BeginPattern(Pattern),int64(0));
	}
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* QTE=World->GetSubsystem<UActionQTESubsystem>();
		auto* Pattern=NewObject<UActionQTEPatternData>(); FActionQTEStep Step;
		Step.Duration=1; Step.PerfectDuration=.2f; Pattern->Steps={Step,Step}; Pattern->Mode=EQTEPatternMode::Rhythm; Pattern->SpawnInterval=.18f;
		TestEqual(TEXT("Overlap duration uses last deadline, not sum"),Pattern->GetDuration(),1.18f);
		const int64 Session=QTE->BeginPattern(Pattern);
		AdvanceBattleTime(World,.9f); TestEqual(TEXT("Overlap visible before first press"),QTE->GetView().Gauges.Num(),2);
		TestTrue(TEXT("First Perfect accepted"),QTE->SubmitInput(Session));
		TestEqual(TEXT("Only resolved diamond disappears"),QTE->GetView().Gauges.Num(),1);
		TestEqual(TEXT("Remaining diamond retains its number"),QTE->GetView().Gauges[0].StepIndex,1);
		AdvanceBattleTime(World,.18f); TestTrue(TEXT("Second Perfect .18 seconds later accepted"),QTE->SubmitInput(Session));
		TestEqual(TEXT("Both independently Perfect"),QTE->GetView().OverallResult,EAttackTimingResult::Success);
		AdvanceBattleTime(World,.2f);
		const int64 Cancel=QTE->BeginPattern(Pattern); AdvanceBattleTime(World,.2f); QTE->CancelQTE(Cancel); AdvanceBattleTime(World,2);
		TestEqual(TEXT("Cancel clears both independent gauges"),QTE->GetView().Gauges.Num(),0);
		TestEqual(TEXT("Cancelled overlap never times out"),QTE->GetView().Result,EAttackTimingResult::Cancelled);
		QTE->BeginPattern(Pattern); AdvanceBattleTime(World,2);
		TestEqual(TEXT("Overlap hitch misses all pending gauges"),QTE->GetView().OverallResult,EAttackTimingResult::Missed);
		Pattern->SpawnInterval=0; TestEqual(TEXT("Zero spawn interval rejected"),QTE->BeginPattern(Pattern),int64(0));
		Pattern->SpawnInterval=.18f; Pattern->Steps[1].Duration=.1f; Pattern->Steps[1].PerfectDuration=.05f;
		TestEqual(TEXT("Reversed gauge deadlines rejected"),QTE->BeginPattern(Pattern),int64(0));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatNumberIntegrationTest,
	"ExpeditionPortfolio.Combat.Feedback.AppliedNumbers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatNumberIntegrationTest::RunTest(const FString& Parameters)
{
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>();
		auto* PlayerHP=AddCombatantComponent(Player); AddCombatantComponent(Enemy);
		PlayerHP->ApplyDamage(5);
		TArray<FCombatNumber> Numbers;
		Battle->OnCombatNumber().AddLambda([&](const FCombatNumber& Number)
		{
			Numbers.Add(Number);
			TestFalse(TEXT("Number listeners cannot submit reentrant commands"),Battle->RequestPlayerBasicAttack());
		});
		Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy);
		TestEqual(TEXT("Battle entry is not a healing event"),Numbers.Num(),0);
		Battle->RequestPlayerBasicAttack();
		if (!TestEqual(TEXT("Basic impact emits once"),Numbers.Num(),1)) { return false; }
		TestEqual(TEXT("Basic damage target"),Numbers.Last().Target.Get(),static_cast<AActor*>(Enemy));
		TestEqual(TEXT("Basic damage is white"),Numbers.Last().Kind,ECombatNumberKind::Damage);
		TestEqual(TEXT("Basic damage value"),Numbers.Last().Amount,25.f);
		CompleteNoInputEnemyTurn(World);
		if (!TestEqual(TEXT("Enemy impact emits once"),Numbers.Num(),2)) { return false; }
		TestEqual(TEXT("Enemy hits player"),Numbers.Last().Target.Get(),static_cast<AActor*>(Player));
		TestEqual(TEXT("Enemy actual damage"),Numbers.Last().Amount,20.f);
		Battle->RequestPlayerCommand(EBattlePlayerCommand::UsePotion);
		if (!TestEqual(TEXT("Potion emits once"),Numbers.Num(),3)) { return false; }
		TestEqual(TEXT("Potion is green"),Numbers.Last().Kind,ECombatNumberKind::Healing);
		TestEqual(TEXT("Healing caps to missing HP"),Numbers.Last().Amount,25.f);
		CompleteNoInputEnemyTurn(World);
		PlayerHP->RestoreBattleHealth(100);
		TestEqual(TEXT("Snapshot restoration emits no combat number"),Numbers.Num(),4);
		Battle->RequestPlayerCommand(EBattlePlayerCommand::UsePotion);
		TestEqual(TEXT("Full HP potion reports zero"),Numbers.Last().Amount,0.f);
		TestEqual(TEXT("Zero healing stays green"),Numbers.Last().Kind,ECombatNumberKind::Healing);
		AdvanceToDefenseWindow(World);
		Battle->RequestDodge(); ExpireDefenseWindow(World);
		TestEqual(TEXT("Dodge emits no fake damage"),Numbers.Num(),5);
	}
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>();
		AddCombatantComponent(Player); auto* EnemyHP=AddCombatantComponent(Enemy); EnemyHP->ApplyDamage(93);
		TArray<FCombatNumber> Numbers;
		Battle->OnCombatNumber().AddLambda([&](const FCombatNumber& Number) { Numbers.Add(Number); });
		Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy);
		Battle->RequestPlayerCommand(EBattlePlayerCommand::MeleeAttack);
		if (!TestEqual(TEXT("Killing hit emits before victory cleanup"),Numbers.Num(),1)) { return false; }
		TestEqual(TEXT("Overkill number matches actual lost HP"),Numbers[0].Amount,7.f);
		TestEqual(TEXT("Killing hit enters victory"),Battle->GetCurrentState(),EBattleState::Victory);
		Battle->ContinueAfterVictory();
		TestEqual(TEXT("Result cleanup emits no healing number"),Numbers.Num(),1);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterVictoryLifetimeTest,
	"ExpeditionPortfolio.Combat.Encounter.VictoryLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEncounterVictoryLifetimeTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
	auto* Trigger=World->SpawnActor<ABattleEncounterTrigger>();
	auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>(); auto* Ally=World->SpawnActor<AActor>();
	AddCombatantComponent(Player); auto* EnemyHP=AddCombatantComponent(Enemy); AddCombatantComponent(Ally);
	TestTrue(TEXT("Spawned encounter has an ID"),Trigger->EncounterId.IsValid());
	const FGuid Id=Trigger->EncounterId; Trigger->EnsureEncounterId();
	TestEqual(TEXT("Ensuring an existing ID preserves it"),Trigger->EncounterId,Id);
	EnemyHP->ApplyDamage(99);
	TestTrue(TEXT("Encounter starts"),Battle->RequestBattleStart(Trigger,Player,Enemy));
	Battle->RegisterBattleAlly(Ally);
	TestFalse(TEXT("Continue before victory rejected"),Battle->ContinueAfterVictory());
	TestTrue(TEXT("Lethal hit"),Battle->RequestPlayerBasicAttack());
	TestFalse(TEXT("Enemy stays in result background"),Enemy->IsActorBeingDestroyed());
	TestFalse(TEXT("Encounter is not completed before Continue"),Trigger->IsEncounterCleared());
	TestTrue(TEXT("Continue finalizes victory"),Battle->ContinueAfterVictory());
	TestTrue(TEXT("Enemy is destroyed"),Enemy->IsActorBeingDestroyed());
	TestFalse(TEXT("Enemy no longer blocks movement"),Enemy->GetActorEnableCollision());
	TestFalse(TEXT("Player is retained"),Player->IsActorBeingDestroyed());
	TestFalse(TEXT("Party ally is retained"),Ally->IsActorBeingDestroyed());
	TestTrue(TEXT("Trigger records completion"),Trigger->IsEncounterCleared());
	Trigger->RearmAfterRetreat();
	TestEqual(TEXT("Completed trigger cannot rearm"),Cast<UPrimitiveComponent>(Trigger->GetRootComponent())->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Repeated Continue rejected"),Battle->ContinueAfterVictory());
	TestNull(TEXT("Enemy references are released"),Battle->GetActiveEnemyActor());
	auto* FreshEnemy=World->SpawnActor<AActor>(); AddCombatantComponent(FreshEnemy);
	TestFalse(TEXT("Direct command cannot bypass cleared encounter"),Battle->RequestBattleStart(Trigger,Player,FreshEnemy));
	TestFalse(TEXT("Rejected unrelated enemy remains alive"),FreshEnemy->IsActorBeingDestroyed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterSessionProgressTest,
	"ExpeditionPortfolio.Combat.Encounter.SessionProgress",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEncounterSessionProgressTest::RunTest(const FString& Parameters)
{
	auto* Instance=NewObject<UGameInstance>();
	auto* Progress=NewObject<UEncounterProgressSubsystem>(Instance);
	const FGuid Id=FGuid::NewGuid(), OtherId=FGuid::NewGuid();
	TestFalse(TEXT("Fresh encounter is available"),Progress->IsCleared(TEXT("/Game/Test/Arena"),Id));
	Progress->MarkCleared(TEXT("/Game/Test/UEDPIE_3_Arena"),Id);
	TestTrue(TEXT("PIE prefix is excluded from map identity"),Progress->IsCleared(TEXT("/Game/Test/Arena"),Id));
	TestTrue(TEXT("Another PIE world resolves the same identity"),Progress->IsCleared(TEXT("/Game/Test/UEDPIE_7_Arena"),Id));
	TestFalse(TEXT("Another map is independent"),Progress->IsCleared(TEXT("/Game/Test/OtherArena"),Id));
	TestFalse(TEXT("Another encounter is independent"),Progress->IsCleared(TEXT("/Game/Test/Arena"),OtherId));
	Progress->MarkCleared(TEXT("/Game/Test/Arena"),Id);
	TestTrue(TEXT("Repeated completion is idempotent"),Progress->IsCleared(TEXT("/Game/Test/Arena"),Id));
	Progress->MarkCleared(NAME_None,OtherId); Progress->MarkCleared(TEXT("/Game/Test/Arena"),FGuid());
	TestFalse(TEXT("Missing map is not a persistent encounter"),Progress->IsCleared(NAME_None,OtherId));
	TestFalse(TEXT("Missing ID is not a persistent encounter"),Progress->IsCleared(TEXT("/Game/Test/Arena"),FGuid()));
	auto* NewSession=NewObject<UEncounterProgressSubsystem>(NewObject<UGameInstance>());
	TestFalse(TEXT("New game session starts fresh"),NewSession->IsCleared(TEXT("/Game/Test/Arena"),Id));
	return true;
}
#endif
