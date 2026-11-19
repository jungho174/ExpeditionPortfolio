// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Tests/BattleTestWorld.h"
#include "UI/BattleCameraRig.h"

using Expedition::BattleTest::FScopedBattleTestWorld;
using Expedition::BattleTest::AddCombatantComponent;
using Expedition::BattleTest::AdvanceBattleTime;
using Expedition::BattleTest::CompleteNoInputEnemyTurn;

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

#endif
