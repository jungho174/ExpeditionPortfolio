#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Combat/EnemyAttackPatternData.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroupedBattleLifecycleTest,"ExpeditionPortfolio.Combat.Groups.Lifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FGroupedBattleLifecycleTest::RunTest(const FString& Parameters)
{
	for (int32 Case=0;Case<7;++Case)
	{
		UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("GroupBattleTest")),GetTransientPackage());
		auto* Battle=World->GetSubsystem<UBattleCoordinator>(); Battle->bContentFreeTest=true; Battle->TestTurnTransitionDelay=0;
		auto Advance=[&](float Seconds) { ++GFrameCounter; World->TimeSeconds+=Seconds; World->GetTimerManager().Tick(Seconds); };
		Advance(0);
		auto Stats=[](AActor* Actor)
		{
			auto* Health=NewObject<UCombatantComponent>(Actor); Actor->AddInstanceComponent(Health); Health->RegisterComponent();
			Health->EnemyAttackPatterns.Add(NewObject<UEnemyAttackPatternData>(Actor)); return Health;
		};
		auto* Leader=World->SpawnActor<APawn>();auto* Ally=World->SpawnActor<APawn>();
		auto* Enemy1=World->SpawnActor<AActor>();auto* Enemy2=World->SpawnActor<AActor>();auto* Enemy3=World->SpawnActor<AActor>();
		auto* LeaderHP=Stats(Leader);auto* AllyHP=Stats(Ally);auto* HP1=Stats(Enemy1);auto* HP2=Stats(Enemy2);Stats(Enemy3);
		auto* Source=World->SpawnActor<AActor>();
		if (Case==0)
		{
			TestFalse(TEXT("Empty group rejected atomically"),Battle->RequestGroupBattleStart(Source,Leader,{},{}));
			TestFalse(TEXT("Duplicate enemy rejected"),Battle->RequestGroupBattleStart(Source,Leader,{Enemy1,Enemy1},{}));
			TestFalse(TEXT("One actor cannot be on both teams"),Battle->RequestGroupBattleStart(Source,Leader,{Enemy1,Ally},{Ally}));
			TestEqual(TEXT("Invalid rosters keep exploration"),Battle->GetCurrentState(),EBattleState::Exploration);
			World->DestroyWorld(false);continue;
		}
		if (Case==1)
		{
			TestTrue(TEXT("2 allies and 3 enemies registered"),Battle->RequestGroupBattleStart(Source,Leader,{Enemy1,Enemy2,Enemy3},{Ally}));
			TestEqual(TEXT("First ally acts first"),Battle->GetActingCombatant(),static_cast<AActor*>(Leader));
			Battle->RequestPlayerBasicAttack();
			TestEqual(TEXT("Second ally follows first"),Battle->GetActingCombatant(),static_cast<AActor*>(Ally));
			Battle->RequestPlayerBasicAttack();
			for (AActor* Enemy:{Enemy1,Enemy2,Enemy3})
			{
				TestEqual(TEXT("Enemies use their formation order"),Battle->GetActingCombatant(),Enemy);
				TestTrue(TEXT("Enemy targets a living party member"),Battle->GetActivePlayerPawn()==Leader || Battle->GetActivePlayerPawn()==Ally);
				Advance(1.01f);Advance(.61f);
			}
			TestEqual(TEXT("Next round returns to first ally"),Battle->GetActingCombatant(),static_cast<AActor*>(Leader));
			TestEqual(TEXT("Round counted once"),Battle->GetBattleRound(),2);
			TestEqual(TEXT("Exactly one hit per enemy"),LeaderHP->GetCurrentHealth()+AllyHP->GetCurrentHealth(),140.f);
		}
		if (Case==2)
		{
			HP1->ApplyDamage(90);
			Battle->RequestGroupBattleStart(Source,Leader,{Enemy1,Enemy2},{});Battle->RequestPlayerBasicAttack();
			TestEqual(TEXT("First KO enemy is skipped, no premature victory"),Battle->GetActingCombatant(),Enemy2);
			TestEqual(TEXT("Surviving enemy attacks"),Battle->GetCurrentState(),EBattleState::EnemyTelegraph);
			Advance(1.01f);Advance(.61f);
			TestTrue(TEXT("Living opponents alone are selectable"),Battle->BeginCommandSelection(EBattlePlayerCommand::BasicAttack));
			TestEqual(TEXT("Only second enemy remains"),Battle->GetTargetCount(),1);
			Battle->CancelTargetSelection();HP2->ApplyDamage(90);Battle->RequestPlayerBasicAttack();
			TestEqual(TEXT("Last KO wins"),Battle->GetCurrentState(),EBattleState::Victory);
			TestFalse(TEXT("Bodies remain until continue"),Enemy1->IsActorBeingDestroyed());
			TestTrue(TEXT("Continue accepted once"),Battle->ContinueAfterVictory());
			TestTrue(TEXT("Whole group destroyed"),Enemy1->IsActorBeingDestroyed() && Enemy2->IsActorBeingDestroyed());
			TestFalse(TEXT("Repeated continue rejected"),Battle->ContinueAfterVictory());
		}
		if (Case==3)
		{
			Battle->RequestGroupBattleStart(Source,Leader,{Enemy1,Enemy2},{});
			Battle->BeginCommandSelection(EBattlePlayerCommand::BasicAttack);Battle->MoveTargetSelection(1);
			TestEqual(TEXT("A/D chooses second enemy"),Battle->GetSelectedTarget(),Enemy2);
			TestTrue(TEXT("Target confirmed"),Battle->ConfirmTargetSelection());
			TestFalse(TEXT("Confirmation cannot consume twice"),Battle->ConfirmTargetSelection());Advance(.5f);
			TestEqual(TEXT("Damage reaches chosen enemy only"),HP2->GetCurrentHealth(),75.f);
			TestEqual(TEXT("Unselected enemy stays healthy"),HP1->GetCurrentHealth(),100.f);
			TestEqual(TEXT("Attack selection never changes enemy turn order"),Battle->GetActingCombatant(),Enemy1);
		}
		if (Case==4)
		{
			Battle->RequestGroupBattleStart(Source,Leader,{Enemy1,Enemy2},{Ally});LeaderHP->ApplyDamage(100);
			TestFalse(TEXT("One fallen ally does not defeat party"),Battle->TryFinishBattle());
			FTSTicker::GetCoreTicker().Tick(.01f);
			TestEqual(TEXT("Next living ally can command"),Battle->GetActivePlayerPawn(),Ally);
			Battle->RequestPlayerBasicAttack();
			TestEqual(TEXT("Enemy locks its attack to living ally"),Battle->GetActivePlayerPawn(),Ally);
			AllyHP->ApplyDamage(100);TestTrue(TEXT("All allies KO finishes battle"),Battle->TryFinishBattle());
			TestEqual(TEXT("Entire party defeat"),Battle->GetCurrentState(),EBattleState::Defeat);
			TestTrue(TEXT("Retry restores complete roster"),Battle->RetryBattle());
			TestEqual(TEXT("Both enemies restored"),Battle->GetBattleEnemies().Num(),2);
			TestEqual(TEXT("Both allies restored"),Battle->GetBattleAllies().Num(),2);
			TestEqual(TEXT("Entry health restored to leader"),LeaderHP->GetCurrentHealth(),100.f);
			TestEqual(TEXT("Entry health restored to ally"),AllyHP->GetCurrentHealth(),100.f);
		}
		if (Case==5)
		{
			HP1->ApplyDamage(60);Battle->RequestGroupBattleStart(Source,Leader,{Enemy1,Enemy2},{});Battle->RequestPlayerBasicAttack();
			Advance(1.01f);Advance(.49f);Battle->RequestParry();Advance(.12f);
			TestTrue(TEXT("Counter defeated its attacker"),HP1->IsDefeated());
			TestEqual(TEXT("Other enemy keeps its turn after counter KO"),Battle->GetActingCombatant(),Enemy2);
			TestEqual(TEXT("Counter is not applied to another enemy"),HP2->GetCurrentHealth(),100.f);
		}
		if (Case==6)
		{
			LeaderHP->ApplyDamage(100);
			TestTrue(TEXT("Persisted KO leader can enter with a living ally"),Battle->RequestGroupBattleStart(Source,Leader,{Enemy1,Enemy2},{Ally}));
			TestEqual(TEXT("First living ally opens the encounter"),Battle->GetActingCombatant(),static_cast<AActor*>(Ally));
			Battle->RequestPlayerBasicAttack();
			TestEqual(TEXT("Enemy never targets KO leader"),Battle->GetActivePlayerPawn(),Ally);
		}
		World->DestroyWorld(false);
	}
	return true;
}
#endif
