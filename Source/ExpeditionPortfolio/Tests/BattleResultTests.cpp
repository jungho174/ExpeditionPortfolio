// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Combat/PartyHealthSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Exploration/Encounters/BattleEncounterTrigger.h"
#include "Exploration/Encounters/EncounterProgressSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Tests/BattleTestWorld.h"

#include <limits>

using Expedition::BattleTest::FScopedBattleTestWorld;
using Expedition::BattleTest::AddCombatantComponent;
using Expedition::BattleTest::AdvanceBattleTime;
using Expedition::BattleTest::AdvanceToDefenseWindow;
using Expedition::BattleTest::ExpireDefenseWindow;
using Expedition::BattleTest::CompleteNoInputEnemyTurn;

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
