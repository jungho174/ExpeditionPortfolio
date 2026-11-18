#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Combat/CharacterCombatData.h"
#include "Combat/CharacterActionExecutor.h"
#include "UI/MagicActionData.h"
#include "UI/MagicPresentationComponent.h"
#include "UI/MeleeActionData.h"
#include "UI/PlayerUtilityPresentationComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterActionRulesTest,"ExpeditionPortfolio.Combat.CharacterActions.Rules",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterActionRulesTest::RunTest(const FString& Parameters)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 auto* Battle=World->GetSubsystem<UBattleCoordinator>();Battle->bContentFreeTest=true;
 auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>();
 auto Stats=[](AActor* Owner) { auto* C=NewObject<UCombatantComponent>(Owner); Owner->AddInstanceComponent(C); C->RegisterComponent(); return C; };
 auto* PlayerHP=Stats(Player); auto* EnemyHP=Stats(Enemy);
 EnemyHP->EnemyAttackPatterns.Add(NewObject<UEnemyAttackPatternData>(Enemy));
 auto* Profile=NewObject<UCharacterCombatData>(); PlayerHP->CharacterActions=Profile;Profile->CharacterId=TEXT("TestHero");
 auto Action=[](FName Id,float Damage,int32 Uses)
 { auto* A=NewObject<UCharacterActionData>(); A->ActionId=Id; A->BaseDamage=Damage; A->UsesPerEncounter=Uses; return A; };
 Profile->BasicAttack=Action(TEXT("Basic"),7,0);
 auto* First=Action(TEXT("SkillA"),11,1); auto* Second=Action(TEXT("SkillB"),13,1);
 Profile->Skills={First,Second}; Profile->ParryCounter=Action(TEXT("Counter"),15,0);
 TestTrue(TEXT("Character action set valid"),Profile->IsValidConfiguration());
 TestTrue(TEXT("Encounter starts"),Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy));
 auto Advance=[&](float Time) { ++GFrameCounter; World->TimeSeconds+=Time; World->GetTimerManager().Tick(Time); };
 Advance(0);
 TestFalse(TEXT("Foreign action cannot enter target selection"),Battle->BeginActionSelection(Action(TEXT("Foreign"),99,0)));
 TestTrue(TEXT("Arbitrary skill enters target selection"),Battle->BeginActionSelection(Second));
 TestFalse(TEXT("Repeated request while selecting rejected"),Battle->RequestPlayerAction(Second));
 TestTrue(TEXT("Selection can cancel"),Battle->CancelTargetSelection());
 TestEqual(TEXT("Cancel consumes no use"),Battle->GetRemainingActionUses(Second),1);
 TestTrue(TEXT("Chosen skill accepted"),Battle->RequestPlayerAction(First));
 TestEqual(TEXT("Skill-specific damage"),EnemyHP->GetCurrentHealth(),89.f);
 TestEqual(TEXT("Used skill exhausted"),Battle->GetRemainingActionUses(First),0);
 TestEqual(TEXT("Other limited skill remains available"),Battle->GetRemainingActionUses(Second),1);
 TestFalse(TEXT("Repeated action during enemy turn rejected"),Battle->RequestPlayerAction(Second));
 Advance(1.01f); Advance(1.01f); Advance(.61f); Advance(1.01f);
 TestEqual(TEXT("Enemy turn returns to command"),Battle->GetCurrentState(),EBattleState::PlayerCommand);
 TestFalse(TEXT("Exhausted skill rejected"),Battle->RequestPlayerAction(First));
 TestTrue(TEXT("Independent limited skill accepted"),Battle->RequestPlayerAction(Second));
 TestEqual(TEXT("Second skill damage independent"),EnemyHP->GetCurrentHealth(),76.f);
 Advance(1.01f); Advance(1.01f); Advance(.61f); Advance(1.01f);
 // Observer edits cannot change already accepted damage, even at the state-change boundary.
 auto Handle=Battle->OnBattleStateChanged().AddLambda([&](EBattleState Before,EBattleState After)
 { if (After==EBattleState::PlayerAction) { Profile->BasicAttack->BaseDamage=99; } });
 TestTrue(TEXT("Basic attack resolves through character data"),Battle->RequestPlayerCommand(EBattlePlayerCommand::MeleeAttack));
 TestEqual(TEXT("Accepted damage is snapshotted"),EnemyHP->GetCurrentHealth(),69.f);
 TestEqual(TEXT("Basic attack unlimited"),Battle->GetRemainingActionUses(Profile->BasicAttack),-1);
 Battle->OnBattleStateChanged().Remove(Handle);
 Profile->Skills.Add(First);
 TestFalse(TEXT("Duplicate action IDs rejected"),Profile->IsValidConfiguration());
 World->DestroyWorld(false);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterActionExecutorsTest,"ExpeditionPortfolio.Combat.CharacterActions.ExecutorSeparation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterActionExecutorsTest::RunTest(const FString& Parameters)
{
 auto* Magic=NewObject<UMagicPresentationComponent>(); auto* Melee=NewObject<UPlayerUtilityPresentationComponent>();
 auto* Spell=NewObject<UMagicActionData>(); auto* Strike=NewObject<UMeleeActionData>();
 TestTrue(TEXT("Spell executor supports spell data independent of command slot"),ICharacterActionExecutor::Execute_SupportsCharacterAction(Magic,Spell));
 TestFalse(TEXT("Spell executor does not claim melee data"),ICharacterActionExecutor::Execute_SupportsCharacterAction(Magic,Strike));
 TestTrue(TEXT("Melee executor supports melee counter or skill data"),ICharacterActionExecutor::Execute_SupportsCharacterAction(Melee,Strike));
 TestFalse(TEXT("Melee executor does not claim spell data"),ICharacterActionExecutor::Execute_SupportsCharacterAction(Melee,Spell));
 TestEqual(TEXT("Unconfigured presentation is rejected"),ICharacterActionExecutor::Execute_GetCharacterActionDuration(Melee,Strike,true),0.f);
 return true;
}
#endif
