// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/BattleTestWorld.h"
#include "UI/MagicActionData.h"
#include "UI/MagicPresentationComponent.h"

#include <limits>

using Expedition::BattleTest::FScopedBattleTestWorld;
using Expedition::BattleTest::AddCombatantComponent;

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
		Battle->PlayerAction.Id = 91;
		Battle->PlayerAction.EffectAmount = 25;
		Battle->PlayerAction.bHitApplied = false;
		auto* Presentation = NewObject<UMagicPresentationComponent>(Player);
		Player->AddInstanceComponent(Presentation);
		Presentation->RegisterComponent();
		Battle->PlayerAction.MagicPresentation = Presentation;
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
			TestTrue(TEXT("Missing marker launches instead of inventing instant damage"), Presentation->ProjectileFlight.IsFlying());
		}
		else { TestTrue(TEXT("Launch accepted"), Presentation->LaunchProjectile()); }
		TestFalse(TEXT("Duplicate launch rejected"), Presentation->LaunchProjectile());
		Presentation->TickComponent(2.0f, LEVELTICK_All, nullptr);
		TestTrue(TEXT("A hitch in the launch frame is not pre-launch flight time"), Presentation->ProjectileFlight.IsFlying());
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
		TestEqual(TEXT("Context cleaned once"), Battle->PlayerAction.Id, int64(0));
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

#endif
