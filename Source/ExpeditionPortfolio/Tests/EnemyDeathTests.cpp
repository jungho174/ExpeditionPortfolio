#if WITH_DEV_AUTOMATION_TESTS
#include "UI/EnemyAttackPresentationComponent.h"
#include "UI/EnemyPresentationData.h"
#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Combat/CombatTargetSelection.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Tests/BattleTestWorld.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"

using Expedition::BattleTest::AdvanceBattleTime;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyDeathLifecycleTest,"ExpeditionPortfolio.Combat.EnemyDeath.Lifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEnemyDeathLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	auto* Battle=World->GetSubsystem<UBattleCoordinator>();
	Battle->bContentFreeTest=true; Battle->TestTurnTransitionDelay=0;
	AdvanceBattleTime(World, 0);
	auto Stats=[](AActor* Actor)
	{
		auto* Health=NewObject<UCombatantComponent>(Actor);Actor->AddInstanceComponent(Health);Health->RegisterComponent();return Health;
	};
	auto* Leader=World->SpawnActor<APawn>();auto* LeaderHP=Stats(Leader);
	auto* Source=World->SpawnActor<AActor>();
	auto* Enemy=World->SpawnActor<AActor>();auto* HP=Stats(Enemy);
	auto* Other=World->SpawnActor<AActor>();Stats(Other);
	auto* Root=NewObject<USceneComponent>(Enemy);Enemy->SetRootComponent(Root);Root->RegisterComponent();
	auto* Mesh=NewObject<USkeletalMeshComponent>(Enemy);Mesh->SetupAttachment(Root);Mesh->RegisterComponent();
	const FTransform Home(FRotator(0,-90,0),FVector(0,0,-90));Mesh->SetRelativeTransform(Home);
	auto* Presentation=NewObject<UEnemyAttackPresentationComponent>(Enemy);
	Enemy->AddInstanceComponent(Presentation);Presentation->RegisterComponent();
	Presentation->Mesh=Mesh;Presentation->Combatant=HP;
	Presentation->HealthHandle=HP->OnHealthChanged().AddUObject(Presentation,&UEnemyAttackPresentationComponent::OnHealthChanged);
	TestTrue(TEXT("Start a two-enemy encounter"),Battle->RequestGroupBattleStart(Source,Leader,{Enemy,Other},{}));
	// A counter can kill a monster while its mesh is still beside the player.
	Presentation->OriginalTransform=Home;Presentation->bSavedTransform=true;
	Presentation->ActiveId=42;Presentation->bTravel=true;
	Mesh->SetRelativeLocation(Home.GetLocation()+FVector(200,0,0));
	const FTransform Impact=Mesh->GetRelativeTransform();
	TestTrue(TEXT("Lethal damage accepted"),HP->ApplyDamage(100));
	TestTrue(TEXT("Death state starts even without optional content"),Presentation->IsDeathPresentationActive());
	TestFalse(TEXT("Dead enemy loses collision immediately"),Enemy->GetActorEnableCollision());
	TestFalse(TEXT("Enemy remains visible for the death interval"),Enemy->IsHidden());
	TestTrue(TEXT("Killing-blow position is preserved"),Mesh->GetRelativeTransform().Equals(Impact));
	TestFalse(TEXT("Dead enemy cannot start another attack"),Presentation->BeginAttack(nullptr,99,Leader));
	TestFalse(TEXT("KO removed from target eligibility immediately"),FCombatTargetSelection::IsEligible(Enemy));
	Presentation->StopPresentation();
	TestTrue(TEXT("Turn cleanup cannot snap corpse home"),Mesh->GetRelativeTransform().Equals(Impact));
	AdvanceBattleTime(World, .5f);TestFalse(TEXT("Fallback death interval is not skipped"),Enemy->IsHidden());
	AdvanceBattleTime(World, .5f);TestTrue(TEXT("Dead enemy disappears after its reaction"),Enemy->IsHidden());
	TestFalse(TEXT("Retained instance supports retry"),Enemy->IsActorBeingDestroyed());
	TestFalse(TEXT("Repeated lethal damage cannot restart death"),HP->ApplyDamage(100));
	FTSTicker::GetCoreTicker().Tick(.01f);
	TestEqual(TEXT("Partial kill does not end encounter"),Battle->GetCurrentState(),EBattleState::PlayerCommand);
	TestTrue(TEXT("Target selection remains available"),Battle->BeginCommandSelection(EBattlePlayerCommand::BasicAttack));
	TestEqual(TEXT("Only living enemy is selectable"),Battle->GetTargetCount(),1);
	Battle->CancelTargetSelection();
	LeaderHP->ApplyDamage(100);Battle->TryFinishBattle();
	TestTrue(TEXT("Retry succeeds after an enemy already disappeared"),Battle->RetryBattle());
	TestEqual(TEXT("Retry restores enemy entry HP"),HP->GetCurrentHealth(),100.f);
	TestFalse(TEXT("Retry restores visibility"),Enemy->IsHidden());
	TestTrue(TEXT("Retry restores collision"),Enemy->GetActorEnableCollision());
	TestTrue(TEXT("Retry restores pre-approach mesh transform"),Mesh->GetRelativeTransform().Equals(Home));
	TestFalse(TEXT("Retry clears death state"),Presentation->IsDeathPresentationActive());
	// Restore before an outstanding disappearance timer expires; stale timers must not hide revived actors.
	HP->ApplyDamage(100);AdvanceBattleTime(World, .1f);HP->RestoreBattleHealth(50);AdvanceBattleTime(World, 2);
	TestFalse(TEXT("Cancelled death timer cannot hide revived enemy"),Enemy->IsHidden());
	TestFalse(TEXT("Ordinary healing cannot revive dead enemy"),[&] { HP->ApplyDamage(100);return HP->RestoreHealth(10); }());
	Presentation->ResetDeathPresentation();HP->RestoreBattleHealth(100);
	Enemy->SetActorHiddenInGame(true);Enemy->SetActorEnableCollision(false);
	HP->ApplyDamage(100);HP->RestoreBattleHealth(100);
	TestTrue(TEXT("Restoration preserves pre-existing hidden state"),Enemy->IsHidden());
	TestFalse(TEXT("Restoration preserves pre-existing collision state"),Enemy->GetActorEnableCollision());
	Presentation->ResetDeathPresentation();HP->OnHealthChanged().Remove(Presentation->HealthHandle);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyDeathFadeTest,"ExpeditionPortfolio.Combat.EnemyDeath.FadeIsolationAndRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEnemyDeathFadeTest::RunTest(const FString& Parameters)
{
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);
	AdvanceBattleTime(World, 0);
	auto* Original=UMaterial::GetDefaultMaterial(MD_Surface);
	auto* MeshAsset=NewObject<USkeletalMesh>(World);MeshAsset->GetMaterials().Add(FSkeletalMaterial(Original));
	auto* Source=UMaterialInstanceDynamic::Create(Original,World);
	Source->SetScalarParameterValue(TEXT("DeathOpacity"),1.f);
	auto* Data=NewObject<UEnemyPresentationData>(World);Data->DeathFadeMaterials={Source};Data->DeathFadeDuration=.8f;
	TArray<UEnemyAttackPresentationComponent*> Presentations;
	TArray<UCombatantComponent*> Health;
	for (int32 Index=0;Index<2;++Index)
	{
		auto* Actor=World->SpawnActor<AActor>();
		auto* Mesh=NewObject<USkeletalMeshComponent>(Actor);Actor->SetRootComponent(Mesh);Mesh->SetSkinnedAssetAndUpdate(MeshAsset,false);Mesh->RegisterComponent();Mesh->SetMaterial(0,Original);
		auto* HP=NewObject<UCombatantComponent>(Actor);Actor->AddInstanceComponent(HP);HP->RegisterComponent();
		auto* P=NewObject<UEnemyAttackPresentationComponent>(Actor);Actor->AddInstanceComponent(P);P->RegisterComponent();
		P->Mesh=Mesh;P->Combatant=HP;P->PresentationData=Data;
		P->HealthHandle=HP->OnHealthChanged().AddUObject(P,&UEnemyAttackPresentationComponent::OnHealthChanged);
		Presentations.Add(P);Health.Add(HP);HP->ApplyDamage(100);
	}
	AdvanceBattleTime(World, 1.f);
	auto* A=Presentations[0];auto* B=Presentations[1];
	TestTrue(TEXT("Fade starts after death reaction"),A->Death.IsFading() && B->Death.IsFading());
	auto* FadeA = Cast<UMaterialInstanceDynamic>(A->Mesh->GetMaterial(0));
	auto* FadeB = Cast<UMaterialInstanceDynamic>(B->Mesh->GetMaterial(0));
	if (!TestNotNull(TEXT("First enemy fade material"), FadeA) || !TestNotNull(TEXT("Second enemy fade material"), FadeB))
	{
		World->DestroyWorld(false); return false;
	}
	TestTrue(TEXT("Each enemy gets a separate fade instance"), FadeA != FadeB);
	A->UpdateDeathFade(.4f);
	float Opacity=0;FadeA->GetScalarParameterValue(FMaterialParameterInfo(TEXT("DeathOpacity")),Opacity);
	TestTrue(TEXT("Fade has a visible intermediate value"),FMath::IsNearlyEqual(Opacity,.5f));
	Source->GetScalarParameterValue(FMaterialParameterInfo(TEXT("DeathOpacity")),Opacity);
	TestEqual(TEXT("Shared material is never modified"),Opacity,1.f);
	FadeB->GetScalarParameterValue(FMaterialParameterInfo(TEXT("DeathOpacity")),Opacity);
	TestEqual(TEXT("Other enemy opacity is independent"),Opacity,1.f);
	A->StopPresentation();TestTrue(TEXT("Turn cleanup does not cancel active fade"),A->IsComponentTickEnabled());
	Health[0]->RestoreBattleHealth(100);
	TestFalse(TEXT("Revive cancels active fade tick"),A->IsComponentTickEnabled());
	TestTrue(TEXT("Revive restores exact original material"),A->Mesh->GetMaterial(0)==Original);
	A->UpdateDeathFade(2.f);TestFalse(TEXT("Stale fade cannot hide revived actor"),A->GetOwner()->IsHidden());
	B->UpdateDeathFade(.8f);TestTrue(TEXT("Only completed fade becomes hidden"),B->GetOwner()->IsHidden());
	TestFalse(TEXT("No fade tick remains on hidden enemy"),B->IsComponentTickEnabled());
	Health[1]->RestoreBattleHealth(100);
	TestTrue(TEXT("Hidden enemy restores original material too"),B->Mesh->GetMaterial(0)==Original);
	Data->DeathFadeDuration=0;Health[0]->ApplyDamage(100);AdvanceBattleTime(World, 1.f);
	TestTrue(TEXT("Zero duration retains immediate-hide option"),A->GetOwner()->IsHidden());
	for (int32 Index=0;Index<2;++Index) { Presentations[Index]->ResetDeathPresentation();Health[Index]->OnHealthChanged().Remove(Presentations[Index]->HealthHandle); }
	World->DestroyWorld(false);return true;
}
#endif
