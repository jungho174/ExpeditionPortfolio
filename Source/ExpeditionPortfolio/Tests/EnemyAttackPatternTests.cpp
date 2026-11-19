#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/EnemyAttackPatternData.h"
#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "UI/AnimNotify_EnemyHit.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyAttackPatternValidationTest,
	"ExpeditionPortfolio.Combat.EnemyAttackPattern.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyAttackPatternValidationTest::RunTest(const FString& Parameters)
{
	UEnemyAttackPatternData* Pattern = NewObject<UEnemyAttackPatternData>();
	FString Reason;
	TestTrue(TEXT("Default pattern valid"), Pattern->ValidatePattern(Reason));
	float* Fields[] = { &Pattern->Damage, &Pattern->TelegraphDuration,
		&Pattern->DefenseWindowDuration, &Pattern->ParrySuccessDuration, &Pattern->SelectionWeight };
	for (float* Field : Fields)
	{
		const float Saved = *Field;
		for (float Invalid : { -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
		{
			*Field = Invalid;
			TestFalse(TEXT("Negative and non-finite data rejected"), Pattern->ValidatePattern(Reason));
			TestFalse(TEXT("Validation explains reason"), Reason.IsEmpty());
		}
		*Field = 0.0f;
		TestEqual(TEXT("Only zero weight is valid"), Pattern->ValidatePattern(Reason), Field == &Pattern->SelectionWeight);
		*Field = Saved;
	}
	Pattern->ParrySuccessDuration = 0.7f;
	TestFalse(TEXT("Parry cannot exceed defense duration"), Pattern->ValidatePattern(Reason));
	Pattern->ParrySuccessDuration = Pattern->DefenseWindowDuration;
	TestTrue(TEXT("Equal parry and defense duration accepted"), Pattern->ValidatePattern(Reason));
	Pattern->PatternId = NAME_None;
	TestFalse(TEXT("Missing identifier rejected"), Pattern->ValidatePattern(Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyAttackPatternSelectionTest,
	"ExpeditionPortfolio.Combat.EnemyAttackPattern.WeightedSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyAttackPatternSelectionTest::RunTest(const FString& Parameters)
{
	UEnemyAttackPatternData* Light = NewObject<UEnemyAttackPatternData>();
	UEnemyAttackPatternData* Heavy = NewObject<UEnemyAttackPatternData>();
	UEnemyAttackPatternData* Disabled = NewObject<UEnemyAttackPatternData>();
	UEnemyAttackPatternData* Invalid = NewObject<UEnemyAttackPatternData>();
	Heavy->SelectionWeight = 3.0f;
	Disabled->SelectionWeight = 0.0f;
	Invalid->Damage = -1.0f;
	TArray<TObjectPtr<UEnemyAttackPatternData>> Patterns = {nullptr, Invalid, Disabled, Light, Heavy};
	FRandomStream Random(5719);
	int32 HeavyCount = 0;
	for (int32 Index = 0; Index < 10000; ++Index)
	{
		const auto* Selected = UEnemyAttackPatternData::SelectPattern(Patterns, Random);
		if (Selected == Heavy) { ++HeavyCount; }
		else if (Selected != Light) { AddError(TEXT("Selected an ineligible pattern")); break; }
	}
	TestTrue(TEXT("Seeded 1:3 selection distribution"), HeavyCount > 7200 && HeavyCount < 7800);
	Light->SelectionWeight = 0.0f;
	Heavy->SelectionWeight = 0.0f;
	TestNull(TEXT("No eligible candidate"), UEnemyAttackPatternData::SelectPattern(Patterns, Random));
	Light->SelectionWeight = 1.0f;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		TestTrue(TEXT("Only eligible candidate always selected"), UEnemyAttackPatternData::SelectPattern(Patterns, Random) == Light);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyAttackTimingTest,
	"ExpeditionPortfolio.Combat.EnemyAttackPattern.AnimationTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyAttackTimingTest::RunTest(const FString& Parameters)
{
	UEnemyAttackPatternData* Pattern = NewObject<UEnemyAttackPatternData>();
	UAnimMontage* Montage = NewObject<UAnimMontage>();
	Montage->SetCompositeLength(6.8f);
	FAnimNotifyEvent& Hit = Montage->Notifies.AddDefaulted_GetRef();
	Hit.Notify = NewObject<UAnimNotify_EnemyHit>(Montage);
	Hit.Link(Montage, 4.7f);
	Pattern->AttackMontage = Montage;
	FString Reason;
	TestTrue(TEXT("Notify-based timing valid"), Pattern->ValidatePattern(Reason));
	TestTrue(TEXT("Notify controls hit time"), FMath::IsNearlyEqual(Pattern->GetHitTime(), Hit.GetTriggerTime()));
	Pattern->HitTimeOffset = -0.25f;
	TestTrue(TEXT("Earlier hit offset allowed"), Pattern->ValidatePattern(Reason));
	const auto Snapshot = Pattern->MakeSnapshot();
	Pattern->HitTimeOffset = 0.3f;
	Pattern->DefenseWindowDuration = 1.0f;
	Pattern->ParrySuccessDuration = 0.4f;
	TestTrue(TEXT("Active hit timeline is copied"), FMath::IsNearlyEqual(Snapshot.HitTime, Hit.GetTriggerTime() - 0.25f));
	TestEqual(TEXT("Active window is copied"), Snapshot.DefenseWindowDuration, 0.6f);
	TestEqual(TEXT("Active parry duration is copied"), Snapshot.ParrySuccessDuration, 0.2f);
	Pattern->HitTimeOffset = -4.0f;
	TestFalse(TEXT("Window cannot begin before montage starts"), Pattern->ValidatePattern(Reason));
	Pattern->HitTimeOffset = 3.0f;
	TestFalse(TEXT("Hit cannot follow montage end"), Pattern->ValidatePattern(Reason));
	Pattern->HitTimeOffset = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN offset rejected"), Pattern->ValidatePattern(Reason));
	Pattern->HitTimeOffset = 0.0f;
	Montage->Notifies.Empty();
	TestTrue(TEXT("Missing marker has valid fallback"), Pattern->ValidatePattern(Reason));
	TestEqual(TEXT("Missing marker uses fallback time"), Pattern->GetHitTime(), Pattern->FallbackHitTime);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyComboValidationTest,
	"ExpeditionPortfolio.Combat.EnemyCombo.Validation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEnemyComboValidationTest::RunTest(const FString& Parameters)
{
	auto* Pattern=NewObject<UEnemyAttackPatternData>(); FString Reason;
	FEnemyAttackHit First; First.FallbackHitTime=.6f; Pattern->Hits.Add(First);
	FEnemyAttackHit Second; Second.FallbackHitTime=1.3f; Pattern->Hits.Add(Second);
	TestTrue(TEXT("Two independent hits valid"),Pattern->ValidatePattern(Reason));
	const auto Snapshot=Pattern->MakeSnapshot();
	Pattern->Hits[1].Damage=99;
	TestEqual(TEXT("Per-hit tuning copied"),Snapshot.Hits[1].Damage,10.f);
	Pattern->Hits[1].FallbackHitTime=.8f;
	TestFalse(TEXT("Overlapping defense windows rejected"),Pattern->ValidatePattern(Reason));
	Pattern->Hits[1].FallbackHitTime=1.3f;
	Pattern->Hits[1].ParrySuccessDuration=.8f;
	TestFalse(TEXT("Per-hit parry cannot exceed defense"),Pattern->ValidatePattern(Reason));
	Pattern->Hits[1].ParrySuccessDuration=.2f;
	Pattern->Hits[1].Damage=std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Non-finite hit damage rejected"),Pattern->ValidatePattern(Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyPatternTurnSnapshotTest,
	"ExpeditionPortfolio.Combat.EnemyAttackPattern.TurnSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyPatternTurnSnapshotTest::RunTest(const FString& Parameters)
{
	auto* Pattern = NewObject<UEnemyAttackPatternData>();
	Pattern->PatternId = TEXT("Combo");
	Pattern->Hits.SetNum(2);
	Pattern->Hits[0].FallbackHitTime = 0.75f;
	Pattern->Hits[1].HitTimeOffset = -0.1f;
	auto* Montage = NewObject<UAnimMontage>();
	Montage->SetCompositeLength(3.0f);
	Pattern->AttackMontage = Montage;
	const auto AddSecondHitMarker = [Montage](float Time)
	{
		auto* Notify = NewObject<UAnimNotify_EnemyHit>(Montage);
		Notify->HitIndex = 1;
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.Notify = Notify;
		Event.Link(Montage, Time);
		return Event.GetTriggerTime();
	};
	const float SecondMarkerTime = AddSecondHitMarker(1.8f);
	auto* Disabled = NewObject<UEnemyAttackPatternData>();
	Disabled->SelectionWeight = 0.0f;
	const TArray<TObjectPtr<UEnemyAttackPatternData>> Candidates = {Disabled, Pattern};
	FRandomStream Random(92);
	FRandomStream Expected(92);
	FEnemyAttackPatternSnapshot InFlight;
	const auto* Selected = UEnemyAttackPatternData::SelectPattern(Candidates, Random, &InFlight);
	TestTrue(TEXT("Only the enabled combo is selected"), Selected == Pattern);
	Expected.GetFraction();
	TestEqual(TEXT("A valid draw consumes one random number"), Random.GetCurrentSeed(), Expected.GetCurrentSeed());
	if (!TestEqual(TEXT("Both hits copied at selection"), InFlight.Hits.Num(), 2)) { return false; }
	TestEqual(TEXT("Unmarked first hit uses its own fallback"), InFlight.Hits[0].HitTime, 0.75f);
	TestTrue(TEXT("Second hit uses indexed marker plus offset"),
		FMath::IsNearlyEqual(InFlight.Hits[1].HitTime, SecondMarkerTime - 0.1f));

	Pattern->Hits[0].FallbackHitTime = 0.65f;
	AddSecondHitMarker(2.1f);
	TArray<FString> Errors;
	FEnemyAttackPatternSnapshot NextTurn = InFlight;
	const auto* NewSelection = UEnemyAttackPatternData::SelectPattern(Candidates, Random, &NextTurn,
		[&Errors](const UEnemyAttackPatternData*, const FString& Reason) { Errors.Add(Reason); });
	TestNull(TEXT("Next turn rejects the new duplicate marker"), NewSelection);
	TestTrue(TEXT("Only the invalid combo is reported"), Errors.Num() == 1 && Errors[0].Contains(TEXT("Hit 2")));
	TestEqual(TEXT("No eligible candidates preserve the random stream"), Random.GetCurrentSeed(), Expected.GetCurrentSeed());
	TestEqual(TEXT("Previous selection is replaced with the fallback"), NextTurn.PatternId, FName(TEXT("BasicAttack")));
	TestEqual(TEXT("An in-flight attack retains the original timing"), InFlight.Hits[0].HitTime, 0.75f);
	return true;
}
#endif
