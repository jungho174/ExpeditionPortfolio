// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatantComponent.h"

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <limits>

namespace
{
	UCombatantComponent* CreateCombatantComponent()
	{
		return NewObject<UCombatantComponent>(GetTransientPackage());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatantComponentInitialStateTest,
	"ExpeditionPortfolio.Combat.CombatantComponent.InitialState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatantComponentInitialStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UCombatantComponent* Combatant = CreateCombatantComponent();

	if (!TestNotNull(TEXT("CombatantComponent should be created"), Combatant))
	{
		return false;
	}

	TestEqual(TEXT("Default MaxHealth should be 100"), Combatant->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("CurrentHealth should start at MaxHealth"), Combatant->GetCurrentHealth(), Combatant->GetMaxHealth());
	TestFalse(TEXT("A new combatant should not be defeated"), Combatant->IsDefeated());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatantComponentDamageApplicationTest,
	"ExpeditionPortfolio.Combat.CombatantComponent.DamageApplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatantComponentDamageApplicationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UCombatantComponent* Combatant = CreateCombatantComponent();

	if (!TestNotNull(TEXT("CombatantComponent should be created"), Combatant))
	{
		return false;
	}

	TestTrue(TEXT("Positive damage should be applied"), Combatant->ApplyDamage(30.0f));
	TestEqual(TEXT("Damage should reduce CurrentHealth"), Combatant->GetCurrentHealth(), 70.0f);
	TestFalse(TEXT("A combatant above zero health should remain active"), Combatant->IsDefeated());

	TestTrue(TEXT("Lethal damage should be applied"), Combatant->ApplyDamage(100.0f));
	TestEqual(TEXT("CurrentHealth should clamp at zero"), Combatant->GetCurrentHealth(), 0.0f);
	TestTrue(TEXT("A combatant at zero health should be defeated"), Combatant->IsDefeated());

	TestFalse(TEXT("Damage after defeat should be rejected"), Combatant->ApplyDamage(10.0f));
	TestEqual(TEXT("Rejected post-defeat damage should preserve zero health"), Combatant->GetCurrentHealth(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatantComponentInvalidDamageTest,
	"ExpeditionPortfolio.Combat.CombatantComponent.InvalidDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatantComponentInvalidDamageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UCombatantComponent* Combatant = CreateCombatantComponent();

	if (!TestNotNull(TEXT("CombatantComponent should be created"), Combatant))
	{
		return false;
	}

	TestFalse(TEXT("Zero damage should be rejected"), Combatant->ApplyDamage(0.0f));
	TestFalse(TEXT("Negative damage should be rejected"), Combatant->ApplyDamage(-10.0f));
	TestFalse(TEXT("Non-finite damage should be rejected"), Combatant->ApplyDamage(std::numeric_limits<float>::infinity()));
	TestEqual(TEXT("Rejected damage should not change health"), Combatant->GetCurrentHealth(), Combatant->GetMaxHealth());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatantComponentDelegateNotificationsTest,
	"ExpeditionPortfolio.Combat.CombatantComponent.DelegateNotifications",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatantComponentDelegateNotificationsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UCombatantComponent* Combatant = CreateCombatantComponent();

	if (!TestNotNull(TEXT("CombatantComponent should be created"), Combatant))
	{
		return false;
	}

	TArray<TPair<float, float>> HealthNotifications;
	int32 DefeatedNotificationCount = 0;

	const FDelegateHandle HealthChangedHandle = Combatant->OnHealthChanged().AddLambda(
		[&HealthNotifications](const float PreviousHealth, const float NewHealth)
		{
			HealthNotifications.Emplace(PreviousHealth, NewHealth);
		});

	const FDelegateHandle DefeatedHandle = Combatant->OnDefeated().AddLambda(
		[&DefeatedNotificationCount]()
		{
			++DefeatedNotificationCount;
		});

	TestFalse(TEXT("Rejected damage should not notify listeners"), Combatant->ApplyDamage(0.0f));
	TestTrue(TEXT("Non-lethal damage should succeed"), Combatant->ApplyDamage(25.0f));
	TestTrue(TEXT("Lethal damage should succeed"), Combatant->ApplyDamage(75.0f));
	TestFalse(TEXT("Post-defeat damage should not notify listeners"), Combatant->ApplyDamage(10.0f));

	TestEqual(TEXT("HealthChanged should run once per successful damage application"), HealthNotifications.Num(), 2);
	TestEqual(TEXT("Defeated should run exactly once"), DefeatedNotificationCount, 1);

	if (HealthNotifications.Num() == 2)
	{
		TestEqual(TEXT("First notification should report the previous health"), HealthNotifications[0].Key, 100.0f);
		TestEqual(TEXT("First notification should report the reduced health"), HealthNotifications[0].Value, 75.0f);
		TestEqual(TEXT("Second notification should report the previous health"), HealthNotifications[1].Key, 75.0f);
		TestEqual(TEXT("Second notification should report zero health"), HealthNotifications[1].Value, 0.0f);
	}

	Combatant->OnHealthChanged().Remove(HealthChangedHandle);
	Combatant->OnDefeated().Remove(DefeatedHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatantHealingTest,
	"ExpeditionPortfolio.Combat.CombatantComponent.Healing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatantHealingTest::RunTest(const FString& Parameters)
{
	auto* HP = CreateCombatantComponent();
	TestFalse(TEXT("Full health cannot heal"), HP->RestoreHealth(10));
	HP->ApplyDamage(60);
	int32 Changes = 0;
	HP->OnHealthChanged().AddLambda([&](float Before, float After) { ++Changes; });
	TestFalse(TEXT("Zero healing rejected"), HP->RestoreHealth(0));
	TestFalse(TEXT("Negative healing rejected"), HP->RestoreHealth(-1));
	TestFalse(TEXT("NaN healing rejected"), HP->RestoreHealth(std::numeric_limits<float>::quiet_NaN()));
	TestFalse(TEXT("Infinite healing rejected"), HP->RestoreHealth(std::numeric_limits<float>::infinity()));
	TestEqual(TEXT("Rejected values have no side effects"), Changes, 0);
	TestTrue(TEXT("Valid healing applied"), HP->RestoreHealth(30));
	TestEqual(TEXT("HP increased"), HP->GetCurrentHealth(), 70.0f);
	TestTrue(TEXT("Overheal clamped"), HP->RestoreHealth(60));
	TestEqual(TEXT("Maximum respected"), HP->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("Exactly one event for each heal"), Changes, 2);
	HP->ApplyDamage(100);
	TestFalse(TEXT("Healing does not revive defeated combatant"), HP->RestoreHealth(30));
	TestEqual(TEXT("Remains defeated"), HP->GetCurrentHealth(), 0.0f);
	return true;
}
#endif
