#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/BattleCoordinator.h"
#include "Combat/CharacterCombatData.h"
#include "Combat/CombatantComponent.h"
#include "Combat/ActionQTESubsystem.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaActionRulesTest,"ExpeditionPortfolio.Combat.Targeting.AreaActions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAreaActionRulesTest::RunTest(const FString& Parameters)
{
	for (int32 Case=0;Case<8;++Case)
	{
		auto* World=UWorld::CreateWorld(EWorldType::Game,false);
		auto* Battle=World->GetSubsystem<UBattleCoordinator>();Battle->bContentFreeTest=true;Battle->TestTurnTransitionDelay=0;
		auto Advance=[&](float Seconds) { ++GFrameCounter;World->TimeSeconds+=Seconds;World->GetTimerManager().Tick(Seconds); };
		Advance(0);
		auto Stats=[](AActor* Owner) { auto* HP=NewObject<UCombatantComponent>(Owner);Owner->AddInstanceComponent(HP);HP->RegisterComponent();HP->EnemyAttackPatterns.Add(NewObject<UEnemyAttackPatternData>(Owner));return HP; };
		auto* Player=World->SpawnActor<APawn>();auto* Ally=World->SpawnActor<APawn>();
		auto* Enemy1=World->SpawnActor<AActor>();auto* Enemy2=World->SpawnActor<AActor>();auto* Dead=World->SpawnActor<AActor>();auto* Outsider=World->SpawnActor<AActor>();
		auto* PlayerHP=Stats(Player);auto* AllyHP=Stats(Ally);auto* HP1=Stats(Enemy1);auto* HP2=Stats(Enemy2);auto* DeadHP=Stats(Dead);auto* OutsideHP=Stats(Outsider);
		DeadHP->ApplyDamage(100);
		auto* Profile=NewObject<UCharacterCombatData>();Profile->CharacterId=TEXT("AreaHero");PlayerHP->CharacterActions=Profile;
		auto Action=[](FName Id) { auto* Value=NewObject<UCharacterActionData>();Value->ActionId=Id;Value->BaseDamage=30;return Value; };
		Profile->BasicAttack=Action(TEXT("Basic"));Profile->ParryCounter=Action(TEXT("Counter"));
		auto* Area=Action(TEXT("Area"));Area->TargetScope=ECharacterActionTargetScope::AllEnemies;Area->UsesPerEncounter=1;Profile->Skills={Area};
		TestTrue(TEXT("Roster starts"),Battle->RequestGroupBattleStart(World->SpawnActor<AActor>(),Player,{Enemy1,Enemy2,Dead},{Ally}));
		if (Case==0 || Case==1)
		{
			TestTrue(TEXT("Area selection opens"),Battle->BeginActionSelection(Area));
			TestTrue(TEXT("All-target mode advertised"),Battle->IsSelectingAllEnemies());
			TestEqual(TEXT("Only living encounter enemies highlighted"),Battle->GetSelectedTargets().Num(),2);
			TestFalse(TEXT("A/D does not change area target"),Battle->MoveTargetSelection(1));
			TestEqual(TEXT("Selection causes no damage"),HP1->GetCurrentHealth(),100.f);
			TestTrue(TEXT("Area can cancel"),Battle->CancelTargetSelection());
			TestEqual(TEXT("Cancel consumes no use"),Battle->GetRemainingActionUses(Area),1);
			Battle->BeginActionSelection(Area);
			if (Case==1) { HP1->ApplyDamage(99);HP2->ApplyDamage(90); }
			const float DamageBefore=Battle->GetBattleResult().DamageDealt;
			bool Reentered=false;bool FinishedTooEarly=false;int32 Numbers=0;
			Battle->OnCombatNumber().AddLambda([&](const FCombatNumber& Number)
			{
				++Numbers;FinishedTooEarly|=Battle->GetCurrentState()!=EBattleState::PlayerAction;
				Reentered|=Battle->NotifyPlayerActionHit(Battle->ActivePlayerActionId);
			});
			if (Case==0)
			{
				Battle->OnBattleStateChanged().AddLambda([&](EBattleState Before,EBattleState After)
				{ if (After==EBattleState::PlayerAction) { Area->TargetScope=ECharacterActionTargetScope::SingleEnemy;Area->BaseDamage=99; } });
			}
			TestTrue(TEXT("Area confirmed"),Battle->ConfirmTargetSelection());
			TestFalse(TEXT("Double confirmation rejected"),Battle->ConfirmTargetSelection());Advance(.5f);
			TestEqual(TEXT("One number per living enemy"),Numbers,2);
			TestFalse(TEXT("Hit delegate cannot reenter batch"),Reentered);
			TestFalse(TEXT("No partial-batch victory"),FinishedTooEarly);
			const auto* CasterUses=Battle->ActionUses.Find(Player);
			TestEqual(TEXT("One use for entire batch belongs to caster"),CasterUses?CasterUses->FindRef(Area->ActionId):0,1);
			TestEqual(TEXT("Dead target remains excluded"),DeadHP->GetCurrentHealth(),0.f);
			TestEqual(TEXT("Other encounters untouched"),OutsideHP->GetCurrentHealth(),100.f);
			TestEqual(TEXT("Party untouched"),PlayerHP->GetCurrentHealth()+AllyHP->GetCurrentHealth(),200.f);
			if (Case==0)
			{
				TestEqual(TEXT("Accepted damage and scope frozen for first"),HP1->GetCurrentHealth(),70.f);
				TestEqual(TEXT("Accepted damage and scope frozen for second"),HP2->GetCurrentHealth(),70.f);
			}
			else
			{
				TestTrue(TEXT("Every living target defeated"),HP1->IsDefeated() && HP2->IsDefeated());
				TestEqual(TEXT("Victory after full batch"),Battle->GetCurrentState(),EBattleState::Victory);
				TestEqual(TEXT("Statistics use actual lost HP"),Battle->GetBattleResult().DamageDealt-DamageBefore,11.f);
			}
		}
		else
		{
			// Exercise the public hit boundary with a held presentation and the real QTE subsystem.
			Battle->TryTransitionTo(EBattleState::PlayerAction);Battle->ActivePlayerActionId=501;
			Battle->ActivePlayerCommand=EBattlePlayerCommand::CharacterAction;Battle->ActiveAction=Area;
			Battle->ActiveTargetScope=ECharacterActionTargetScope::AllEnemies;Battle->ActionTargets={Enemy1,Enemy2,Dead};Battle->PendingPlayerDamage=Case==6?0:30;
			Battle->BeginPlayerPreparation(501,1,.2f,1.5f,true);
			TestFalse(TEXT("Preparation has no early damage"),Battle->NotifyPlayerActionHit(501));
			if (Case==2 || Case==5 || Case==7) { World->TimeSeconds+=.9f;TestTrue(TEXT("Perfect accepted once"),Battle->RequestAttackTimingInput()); }
			else if (Case==3) { World->TimeSeconds+=.1f;TestTrue(TEXT("Regular accepted"),Battle->RequestAttackTimingInput()); }
			if (Case==5) { HP1->ApplyDamage(100);FTSTicker::GetCoreTicker().Tick(.01f); }
			if (Case==7)
			{
				Battle->NotifyPlayerActionFinished(501,true);
				TestEqual(TEXT("Interrupted QTE never invents damage"),HP2->GetCurrentHealth(),100.f);
				TestFalse(TEXT("Late hit rejected"),Battle->NotifyPlayerActionHit(501));
			}
			else
			{
				int32 Numbers=0;ECombatNumberKind LastKind=ECombatNumberKind::Healing;
				Battle->OnCombatNumber().AddLambda([&](const FCombatNumber& Number) { ++Numbers;LastKind=Number.Kind; });
				Battle->NotifyPlayerPreparationFinished(501);
				TestTrue(TEXT("Whole batch accepted"),Battle->NotifyPlayerActionHit(501));
				const float Expected=Case==6?100:Case==2 || Case==5?55:70;
				TestEqual(TEXT("QTE result applies equally to survivors"),HP2->GetCurrentHealth(),Expected);
				TestEqual(TEXT("Every eligible target has one number"),Numbers,Case==5?1:2);
				if (Case==2 || Case==5) { TestEqual(TEXT("Perfect numbers use bonus style"),LastKind,ECombatNumberKind::BonusDamage); }
				TestFalse(TEXT("Duplicate notify cannot damage again"),Battle->NotifyPlayerActionHit(501));
				TestEqual(TEXT("Repeat leaves health intact"),HP2->GetCurrentHealth(),Expected);
				Battle->NotifyPlayerActionFinished(501,false);
				TestEqual(TEXT("Other ally gets next turn exactly once"),Battle->GetActingCombatant(),static_cast<AActor*>(Ally));
			}
		}
		World->DestroyWorld(false);
	}
	return true;
}
#endif
