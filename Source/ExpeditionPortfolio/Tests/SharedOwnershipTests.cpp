#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/BattleCoordinator.h"
#include "Combat/BattleRulesData.h"
#include "Combat/CharacterCombatData.h"
#include "Combat/CombatantComponent.h"
#include "Combat/CombatantVisualData.h"
#include "Combat/BattleEnemyActor.h"
#include "Combat/PartyHealthSubsystem.h"
#include "Combat/PlayerCommandData.h"
#include "UI/ActionCameraData.h"
#include "UI/BattleCameraRig.h"
#include "UI/MagicActionData.h"
#include "UI/MagicPresentationComponent.h"
#include "UI/PlayerUtilityPresentationComponent.h"
#include "UI/ItemPresentationComponent.h"
#include "ExpeditionPortfolioCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Tests/BattleTestWorld.h"

using Expedition::BattleTest::AdvanceBattleTime;

namespace
{
	UCombatantComponent* AddStats(AActor* Actor)
	{ auto* Stats=NewObject<UCombatantComponent>(Actor);Actor->AddInstanceComponent(Stats);Stats->RegisterComponent();return Stats; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionContentValidationTest,"ExpeditionPortfolio.Combat.Ownership.ProductionRejectsMissingContent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProductionContentValidationTest::RunTest(const FString& Parameters)
{
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Battle=World->GetSubsystem<UBattleCoordinator>();
	auto* Player=World->SpawnActor<APawn>();auto* Enemy=World->SpawnActor<AActor>();auto* PlayerStats=AddStats(Player);auto* EnemyStats=AddStats(Enemy);
	TestFalse(TEXT("No-content simulation is disabled by default"),Battle->IsContentFreeTest());
	TestFalse(TEXT("Missing profile does not start a default mage battle"),Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy));
	TestEqual(TEXT("Rejected start remains in exploration"),Battle->GetCurrentState(),EBattleState::Exploration);
	auto* Profile=NewObject<UCharacterCombatData>();Profile->CharacterId=TEXT("Hero");PlayerStats->CharacterActions=Profile;
	auto* Action=NewObject<UMagicActionData>();Action->ActionId=TEXT("Hero.Attack");Action->AttackMontage=NewObject<UAnimMontage>();Action->AttackMontage->SetCompositeLength(1);
	Profile->BasicAttack=Action;Profile->ParryCounter=Action;
	TestFalse(TEXT("Valid action data without executor is rejected"),Battle->ValidateCharacterSetup(Player));
	auto* Executor=NewObject<UMagicPresentationComponent>(Player);Player->AddInstanceComponent(Executor);Executor->RegisterComponent();
	TestTrue(TEXT("A matching executor validates the configured action"),Battle->ValidateCharacterSetup(Player));
	Battle->ActivePlayerPawn=Player;Battle->ActiveEnemyActor=Enemy;Battle->PlayerCombatant=PlayerStats;Battle->EnemyCombatant=EnemyStats;
	Battle->ActiveEncounterSource=World->SpawnActor<AActor>();Battle->CurrentState=EBattleState::PlayerCommand;
	Action->ExecutorComponentName=TEXT("MissingExecutor");
	TestFalse(TEXT("Missing executor cannot consume a live command"),Battle->RequestPlayerAction(Action));
	TestEqual(TEXT("Missing animation cannot manufacture default damage"),EnemyStats->GetCurrentHealth(),100.f);
	World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedItemRulesTest,"ExpeditionPortfolio.Combat.Ownership.SharedItemsAndRetry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSharedItemRulesTest::RunTest(const FString& Parameters)
{
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Battle=World->GetSubsystem<UBattleCoordinator>();Battle->bContentFreeTest=true;
	AdvanceBattleTime(World, 0);
	auto* Player=World->SpawnActor<APawn>();auto* Enemy=World->SpawnActor<AActor>();auto* Ally=World->SpawnActor<AActor>();
	auto* PlayerHP=AddStats(Player);auto* EnemyHP=AddStats(Enemy);auto* AllyHP=AddStats(Ally);EnemyHP->EnemyAttackPatterns.Add(NewObject<UEnemyAttackPatternData>());
	PlayerHP->ApplyDamage(40);AllyHP->ApplyDamage(50);
	auto* Rules=NewObject<UBattleRulesData>();Rules->TurnTransitionDelay=0;
	auto* Item=NewObject<UCombatItemData>(Rules);Item->ItemId=TEXT("Shared.Potion");Item->HealingAmount=12;
	FCombatItemStock Stock;Stock.Item=Item;Stock.Quantity=2;Rules->InitialItems.Add(Stock);
	auto* OldCharacterRules=NewObject<UPlayerCommandData>();OldCharacterRules->PotionHealing=99;OldCharacterRules->PotionsPerEncounter=77;PlayerHP->PlayerCommandData=OldCharacterRules;
	TestTrue(TEXT("Common rules configured"),Battle->ConfigureRules(Rules));
	TestTrue(TEXT("Start battle"),Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy));
	TestTrue(TEXT("Register a second ally"),Battle->RegisterBattleAlly(Ally));
	TestEqual(TEXT("Party stock overrides retired character stock"),Battle->GetRemainingItems(Item),2);
	TestTrue(TEXT("Item target selection begins"),Battle->BeginItemSelection(Item));
	for (int32 I=0;I<3 && Battle->GetSelectedTarget()!=Ally;++I) { Battle->MoveTargetSelection(1); }
	TestTrue(TEXT("Ally selected"),Battle->GetSelectedTarget()==Ally);
	TestTrue(TEXT("Cancel accepted"),Battle->CancelTargetSelection());TestEqual(TEXT("Cancel preserves party stock"),Battle->GetRemainingItems(Item),2);
	TestTrue(TEXT("Select item again"),Battle->BeginItemSelection(Item));
	for (int32 I=0;I<3 && Battle->GetSelectedTarget()!=Ally;++I) { Battle->MoveTargetSelection(1); }
	TestTrue(TEXT("Confirm ally use"),Battle->ConfirmTargetSelection());TestFalse(TEXT("Duplicate confirmation rejected"),Battle->ConfirmTargetSelection());
	AdvanceBattleTime(World, .48f);TestEqual(TEXT("Shared item definition controls healing"),AllyHP->GetCurrentHealth(),62.f);TestEqual(TEXT("One shared item consumed"),Battle->GetRemainingItems(Item),1);
	AdvanceBattleTime(World, 1.01f);AdvanceBattleTime(World, .61f);
	TestTrue(TEXT("Same shared item available for self"),Battle->BeginItemSelection(Item));
	for (int32 I=0;I<3 && Battle->GetSelectedTarget()!=Player;++I) { Battle->MoveTargetSelection(1); }
	Battle->ConfirmTargetSelection();AdvanceBattleTime(World, .48f);TestEqual(TEXT("Different recipient consumes the same pool"),Battle->GetRemainingItems(Item),0);
	AdvanceBattleTime(World, 1.01f);AdvanceBattleTime(World, .61f);TestFalse(TEXT("Empty common stock cannot be used"),Battle->CanUseItem(Item));
	PlayerHP->ApplyDamage(999);Battle->TryFinishBattle();TestTrue(TEXT("Retry succeeds"),Battle->RetryBattle());
	TestEqual(TEXT("Retry restores entry item pool"),Battle->GetRemainingItems(Item),2);
	TestEqual(TEXT("Retry restores actor's entry HP"),PlayerHP->GetCurrentHealth(),60.f);
	TestEqual(TEXT("Retry restores ally entry HP"),AllyHP->GetCurrentHealth(),50.f);
	Rules->InitialItems.Add(Stock);TestFalse(TEXT("Duplicate item IDs rejected"),Rules->IsValidConfiguration());
	World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyIdentityOwnershipTest,"ExpeditionPortfolio.Combat.Ownership.PartyIdentityAndRepeatedEnemies",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPartyIdentityOwnershipTest::RunTest(const FString& Parameters)
{
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Store=NewObject<UPartyHealthSubsystem>(NewObject<UGameInstance>());
	auto* A=AddStats(World->SpawnActor<AActor>());auto* B=AddStats(World->SpawnActor<AActor>());
	auto* Profile=NewObject<UCharacterCombatData>();Profile->CharacterId=TEXT("Mage");A->CharacterActions=Profile;B->CharacterActions=Profile;
	A->Persistence=ECombatantPersistence::UniquePartyMember;B->Persistence=ECombatantPersistence::UniquePartyMember;
	TestTrue(TEXT("First party ID registers"),Store->RegisterCharacter(A->GetCharacterId(),A));Store->StoreHealth(A->GetCharacterId(),37);
	AddExpectedError(TEXT("Duplicate party character ID"),EAutomationExpectedErrorFlags::Contains,1);
	TestFalse(TEXT("Another live owner cannot reuse party save key"),Store->RegisterCharacter(B->GetCharacterId(),B));
	Store->UnregisterCharacter(A->GetCharacterId(),A);TestTrue(TEXT("Replacement after world teardown can reuse character ID"),Store->RegisterCharacter(B->GetCharacterId(),B));
	float HP=0;TestTrue(TEXT("Replacement finds preserved HP"),Store->FindHealth(B->GetCharacterId(),HP));TestEqual(TEXT("Stable character ID keeps HP"),HP,37.f);
	auto* EnemyA=World->SpawnActor<ABattleEnemyActor>();auto* EnemyB=World->SpawnActor<ABattleEnemyActor>();
	EnemyA->Combatant->CharacterActions=Profile;EnemyB->Combatant->CharacterActions=Profile;
	TestTrue(TEXT("Repeated enemy definitions do not become party save keys"),EnemyA->Combatant->GetCharacterId().IsNone() && EnemyB->Combatant->GetCharacterId().IsNone());
	EnemyA->Combatant->ApplyDamage(12);TestEqual(TEXT("Enemy instance HP remains independent"),EnemyB->Combatant->GetCurrentHealth(),100.f);
	World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatantAnchorsTest,"ExpeditionPortfolio.Combat.Ownership.ActorSpecificAnchors",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCombatantAnchorsTest::RunTest(const FString& Parameters)
{
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Actor=World->SpawnActor<AActor>();
	auto* Box=NewObject<UBoxComponent>(Actor);Actor->SetRootComponent(Box);Box->SetBoxExtent(FVector(10,20,30));Box->RegisterComponent();Actor->SetActorLocation(FVector(100,200,300));
	auto* Stats=AddStats(Actor);auto* Visual=NewObject<UCombatantVisualData>();Stats->VisualData=Visual;
	Visual->DamageNumber.WorldOffset=FVector(1,2,35);Visual->TargetMarker.bBoundsTopFallback=true;Visual->TargetMarker.BoundsTopOffset=28;
	TestTrue(TEXT("Damage position belongs to the recipient"),UCombatantVisualData::ResolveAnchor(Actor,ECombatantAnchor::DamageNumber).Equals(FVector(101,202,335)));
	TestTrue(TEXT("Unknown skeleton uses configured bounds fallback"),UCombatantVisualData::ResolveAnchor(Actor,ECombatantAnchor::TargetMarker).Equals(FVector(100,200,358)));
	Visual->ParryComplete.WorldOffset=FVector(50,0,0);TestTrue(TEXT("Parry origin is independently configured"),UCombatantVisualData::ResolveAnchor(Actor,ECombatantAnchor::ParryComplete).Equals(FVector(150,200,300)));
	World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionCameraOwnershipTest,"ExpeditionPortfolio.Combat.Ownership.PerActionCameras",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FActionCameraOwnershipTest::RunTest(const FString& Parameters)
{
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Player=World->SpawnActor<AActor>();auto* Enemy=World->SpawnActor<AActor>();auto* Rig=World->SpawnActor<ABattleCameraRig>();
	auto* A=NewObject<UCharacterActionData>();auto* B=NewObject<UCharacterActionData>();A->CameraData=NewObject<UActionCameraData>();B->CameraData=NewObject<UActionCameraData>();
	A->CameraData->PreparationShot.FieldOfView=42;B->CameraData->PreparationShot.FieldOfView=67;
	A->CameraData->PreparationShot.bAvoidWorldObstacles=false;B->CameraData->PreparationShot.bAvoidWorldObstacles=false;
	FBattleCameraShot Common;Common.FieldOfView=55;Common.bAvoidWorldObstacles=false;
	Rig->ConfigureShot(UActionCameraData::ResolveShot(A,true,Common),Player,Enemy);TestEqual(TEXT("First skill controls preparation camera"),Rig->GetCameraComponent()->FieldOfView,42.f);
	Rig->ConfigureShot(UActionCameraData::ResolveShot(B,true,Common),Player,Enemy);TestEqual(TEXT("Second skill has independent camera"),Rig->GetCameraComponent()->FieldOfView,67.f);
	B->CameraData->bOverrideExecution=false;Rig->ConfigureShot(UActionCameraData::ResolveShot(B,false,Common),Player,Enemy);TestEqual(TEXT("Unspecified phase uses shared camera behavior"),Rig->GetCameraComponent()->FieldOfView,55.f);
	TestEqual(TEXT("Per-skill configuration does not mutate common default"),Common.FieldOfView,55.f);
	World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCompositionTest,"ExpeditionPortfolio.Combat.Ownership.NeutralBaseActors",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCombatCompositionTest::RunTest(const FString& Parameters)
{
	const auto* Character=GetDefault<AExpeditionPortfolioCharacter>();
	TestNull(TEXT("Base character does not force magic execution"),Character->FindComponentByClass<UMagicPresentationComponent>());
	TestNull(TEXT("Base character does not force melee execution"),Character->FindComponentByClass<UPlayerUtilityPresentationComponent>());
	TestNotNull(TEXT("Shared item capability remains available"),Character->FindComponentByClass<UItemPresentationComponent>());
	const auto* Enemy=GetDefault<ABattleEnemyActor>();TestNull(TEXT("Common enemy has no species mesh asset"),Enemy->GetMesh()->GetSkeletalMeshAsset());
	TestTrue(TEXT("Common enemy has no species patterns"),Enemy->Combatant->EnemyAttackPatterns.IsEmpty());return true;
}
#endif
