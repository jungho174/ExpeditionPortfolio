// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Combat/PlayerCommandData.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Tests/BattleTestWorld.h"

using Expedition::BattleTest::FScopedBattleTestWorld;
using Expedition::BattleTest::AddCombatantComponent;
using Expedition::BattleTest::AdvanceBattleTime;
using Expedition::BattleTest::CompleteNoInputEnemyTurn;

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
		Coordinator->PlayerAction.Id = 41;
		Coordinator->PlayerAction.EffectAmount = 25.0f;
		Coordinator->PlayerAction.bHitApplied = false;
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
			World->GetTimerManager().SetTimer(Coordinator->PlayerAction.TimeoutTimer,
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
		TestEqual(TEXT("Action context cleared"), Coordinator->PlayerAction.Id, int64(0));
		TestFalse(TEXT("Late hit rejected"), Coordinator->NotifyPlayerActionHit(41));
		TestFalse(TEXT("Duplicate completion rejected"), Coordinator->NotifyPlayerActionFinished(41, false));
		TestEqual(TEXT("Next flow reached"), Coordinator->GetCurrentState(),
			(Case == 4 || Case == 5) ? EBattleState::Exploration : EBattleState::EnemyTelegraph);
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullHealthPotionTest,
	"ExpeditionPortfolio.Combat.Items.FullHealthPotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullHealthPotionTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope(1.0f);auto* World=Scope.GetWorld();auto* Battle=Scope.GetCoordinator();
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

#endif
