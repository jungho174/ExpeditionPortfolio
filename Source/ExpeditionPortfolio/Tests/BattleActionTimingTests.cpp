// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/ActionQTESubsystem.h"
#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Tests/BattleTestWorld.h"

#include <limits>

using Expedition::BattleTest::FScopedBattleTestWorld;
using Expedition::BattleTest::AddCombatantComponent;
using Expedition::BattleTest::AdvanceBattleTime;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingRulesTest,
	"ExpeditionPortfolio.Combat.AttackTiming.Rules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAttackTimingRulesTest::RunTest(const FString& Parameters)
{
	FAttackTimingWindow Window;
	TestFalse(TEXT("Zero duration rejected"), Window.Begin(0,0,.3f,1.5f));
	TestFalse(TEXT("Window beyond preparation rejected"), Window.Begin(0,2.5f,3,1.5f));
	TestFalse(TEXT("Nonfinite multiplier rejected"), Window.Begin(0,2.5f,.3f,std::numeric_limits<float>::quiet_NaN()));
	TestTrue(TEXT("Start valid window"), Window.Begin(10,2.5f,.3f,1.5f));
	TestFalse(TEXT("Input before action rejected"), Window.Submit(9));
	TestTrue(TEXT("Early input consumes attempt"), Window.Submit(12.19));
	TestEqual(TEXT("White input is Regular"), Window.Result,EAttackTimingResult::Failed);
	TestFalse(TEXT("Cannot retry early failure"), Window.Submit(12.3));
	Window.Begin(10,2.5f,.3f,1.5f);
	TestTrue(TEXT("Opening boundary included"), Window.Submit(12.2));
	TestEqual(TEXT("Boundary succeeds"),Window.Result,EAttackTimingResult::Success);
	TestEqual(TEXT("Success multiplier"),Window.DamageMultiplier(),1.5f);
	Window.Close(true);
	TestEqual(TEXT("Cancellation removes uncommitted bonus"),Window.DamageMultiplier(),1.0f);
	Window.Begin(10,2.5f,.3f,1.5f);
	TestFalse(TEXT("Closing boundary excluded"),Window.Submit(12.5));
	Window.Close();
	TestEqual(TEXT("No input marked missed"),Window.Result,EAttackTimingResult::Missed);
	TestFalse(TEXT("Closed window rejects input"),Window.Submit(12.3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingIntegrationTest,
	"ExpeditionPortfolio.Combat.AttackTiming.ActionLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAttackTimingIntegrationTest::RunTest(const FString& Parameters)
{
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>();
		AddCombatantComponent(Player); auto* HP=AddCombatantComponent(Enemy);
		Battle->RequestBattleStart(World->SpawnActor<AActor>(),Player,Enemy); Battle->TryTransitionTo(EBattleState::PlayerAction);
		Battle->PlayerAction.Id=101; Battle->PlayerAction.Command=EBattlePlayerCommand::BasicAttack; Battle->PlayerAction.EffectAmount=17;
		Battle->BeginPlayerPreparation(101,1,.2f,1.7f,true);
		TArray<FCombatNumber> Numbers;
		Battle->OnCombatNumber().AddLambda([&](const FCombatNumber& Number) { Numbers.Add(Number); });
		World->GetSubsystem<UActionQTESubsystem>()->Timing.StartedAt-=.9;
		TestTrue(TEXT("QTE-enabled non-strong action accepts input"),Battle->RequestAttackTimingInput());
		Battle->NotifyPlayerPreparationFinished(101); Battle->NotifyPlayerActionHit(101);
		TestTrue(TEXT("Generic QTE bonus applies without PowerStrike check"),FMath::IsNearlyEqual(HP->GetCurrentHealth(),71.1f));
		TestEqual(TEXT("QTE impact has one number"),Numbers.Num(),1);
		if (Numbers.Num()==1)
		{
			TestEqual(TEXT("QTE multiplier marks bonus color"),Numbers[0].Kind,ECombatNumberKind::BonusDamage);
			TestTrue(TEXT("Bonus number matches actual loss"),FMath::IsNearlyEqual(Numbers[0].Amount,28.9f,.001f));
		}
		TestFalse(TEXT("Duplicate impact has no second number"),Battle->NotifyPlayerActionHit(101));
		TestEqual(TEXT("Number count after duplicate"),Numbers.Num(),1);
	}
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* Battle=Scope.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>(); auto* Encounter=World->SpawnActor<AActor>();
		AddCombatantComponent(Player); auto* HP=AddCombatantComponent(Enemy);
		Battle->RequestBattleStart(Encounter,Player,Enemy); Battle->TryTransitionTo(EBattleState::PlayerAction);
		Battle->PlayerAction.Id=81; Battle->PlayerAction.Command=EBattlePlayerCommand::PowerStrike; Battle->PlayerAction.EffectAmount=40;
		Battle->BeginPlayerPreparation(81,1,.2f,1.5f,true); World->GetSubsystem<UActionQTESubsystem>()->Timing.StartedAt-=.9;
		TestTrue(TEXT("Timing success accepted"),Battle->RequestAttackTimingInput());
		AdvanceBattleTime(World,1.01f);
		TestFalse(TEXT("Gauge closes independently of preparation"),Battle->GetAttackTimingView().bActive);
		TestEqual(TEXT("Success survives post-gauge hold"),Battle->GetAttackTimingView().Result,EAttackTimingResult::Success);
		TestEqual(TEXT("Closing gauge does not launch attack"),Battle->GetPlayerActionPhase(),EPlayerActionPhase::Preparing);
		TestFalse(TEXT("Hold rejects another timing input"),Battle->RequestAttackTimingInput());
		TestFalse(TEXT("Hold rejects premature hit"),Battle->NotifyPlayerActionHit(81));
		TestEqual(TEXT("Hold preserves HP"),HP->GetCurrentHealth(),100.f);
		Battle->NotifyPlayerPreparationFinished(81);
		TestTrue(TEXT("Release accepts hit"),Battle->NotifyPlayerActionHit(81));
		TestEqual(TEXT("Bonus remains after hold"),HP->GetCurrentHealth(),40.f);
	}
	for (int32 Case=0; Case<5; ++Case)
	{
		FScopedBattleTestWorld TestWorld;
		auto* World=TestWorld.GetWorld(); auto* Battle=TestWorld.GetCoordinator();
		auto* Player=World->SpawnActor<APawn>(); auto* Enemy=World->SpawnActor<AActor>(); auto* Encounter=World->SpawnActor<AActor>();
		AddCombatantComponent(Player); auto* HP=AddCombatantComponent(Enemy);
		TestTrue(TEXT("Start"),Battle->RequestBattleStart(Encounter,Player,Enemy));
		Battle->TryTransitionTo(EBattleState::PlayerAction);
		Battle->PlayerAction.Id=31;
		Battle->PlayerAction.Command=EBattlePlayerCommand::PowerStrike;
		Battle->PlayerAction.EffectAmount=40;
		Battle->BeginPlayerPreparation(31,2.5f,.3f,1.5f,true);
		TestTrue(TEXT("View exposes timing window"),Battle->GetAttackTimingView().bActive);
		Battle->NotifyPlayerPreparationFinished(30);
		TestEqual(TEXT("Stale completion ignored"),Battle->GetPlayerActionPhase(),EPlayerActionPhase::Preparing);
		if (Case!=2)
		{
			if (Case!=1) { World->GetSubsystem<UActionQTESubsystem>()->Timing.StartedAt-=2.3; }
			bool Reentered=true;
			auto Handle=Battle->OnAttackTimingChanged().AddLambda([&]{ Reentered=Battle->RequestAttackTimingInput(); });
			TestTrue(TEXT("First input accepted"),Battle->RequestAttackTimingInput());
			TestFalse(TEXT("Result event reentry rejected"),Reentered);
			Battle->OnAttackTimingChanged().Remove(Handle);
			TestFalse(TEXT("Repeated input rejected"),Battle->RequestAttackTimingInput());
		}
		TestFalse(TEXT("Bonus input cannot apply damage during preparation"),Battle->NotifyPlayerActionHit(31));
		TestEqual(TEXT("Preparation leaves HP untouched"),HP->GetCurrentHealth(),100.0f);
		if (Case==3 || Case==4)
		{
			if (Case==4)
			{
				AddExpectedError(TEXT("completion timed out"),EAutomationExpectedErrorFlags::Contains,1);
				Battle->HandlePlayerActionTimeout(31);
			}
			else { Battle->NotifyPlayerActionFinished(31,true); }
			TestEqual(TEXT("Interrupted success never invents a hit"),HP->GetCurrentHealth(),100.0f);
			TestFalse(TEXT("Interrupted window closed"),Battle->GetAttackTimingView().bActive);
		}
		else
		{
			Battle->NotifyPlayerPreparationFinished(31);
			TestTrue(TEXT("Attack hit accepted"),Battle->NotifyPlayerActionHit(31));
			TestEqual(TEXT("Only successful action receives bonus"),HP->GetCurrentHealth(),Case==0 ? 40.0f : 60.0f);
			TestFalse(TEXT("No duplicate damage"),Battle->NotifyPlayerActionHit(31));
			Battle->NotifyPlayerActionFinished(31,false);
		}
		TestFalse(TEXT("Next phase rejects old timing input"),Battle->RequestAttackTimingInput());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionQTESessionsTest,
	"ExpeditionPortfolio.Combat.QTE.Sessions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FActionQTESessionsTest::RunTest(const FString& Parameters)
{
	FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* QTE=World->GetSubsystem<UActionQTESubsystem>();
	int32 Starts=0, Results=0, Cancels=0; int64 Reentry=-1;
	QTE->Started.AddLambda([&](int64) { ++Starts; Reentry=QTE->BeginQTE(1,.2f); });
	QTE->Resolved.AddLambda([&](int64 Id, EAttackTimingResult) { ++Results; TestFalse(TEXT("Result callback cannot resubmit"),QTE->SubmitInput(Id)); });
	QTE->Cancelled.AddLambda([&](int64) { ++Cancels; });
	TestEqual(TEXT("Invalid settings emit nothing"),QTE->BeginQTE(0,.2f),int64(0));
	const int64 Regular=QTE->BeginQTE(1,.2f);
	TestTrue(TEXT("Session created"),Regular>0);
	TestEqual(TEXT("Start reentry blocked"),Reentry,int64(0));
	TestEqual(TEXT("Active session cannot be overwritten"),QTE->BeginQTE(1,.2f),int64(0));
	TestTrue(TEXT("White input accepted"),QTE->SubmitInput(Regular));
	TestEqual(TEXT("White grade is Regular"),QTE->GetView().Result,EAttackTimingResult::Failed);
	TestFalse(TEXT("No repeated input"),QTE->SubmitInput(Regular));
	AdvanceBattleTime(World,1.01f);
	TestEqual(TEXT("Closing Regular does not emit Miss"),Results,1);
	const int64 Perfect=QTE->BeginQTE(1,.2f,1.8f);
	TestTrue(TEXT("Monotonic session tokens"),Perfect>Regular);
	TestFalse(TEXT("Stale input rejected"),QTE->SubmitInput(Regular));
	QTE->Timing.StartedAt-=.9;
	QTE->SubmitInput(Perfect); QTE->FinishWindow(Perfect);
	TestEqual(TEXT("Perfect grade"),QTE->GetView().Result,EAttackTimingResult::Success);
	TestEqual(TEXT("Per-session multiplier"),QTE->GetDamageMultiplier(Perfect),1.8f);
	TestEqual(TEXT("Old session cannot lend bonus"),QTE->GetDamageMultiplier(Regular),1.f);
	const int64 Miss=QTE->BeginQTE(.1f,.02f);
	AdvanceBattleTime(World,.11f);
	TestEqual(TEXT("Timeout resolves Miss without a widget"),QTE->GetView().Result,EAttackTimingResult::Missed);
	TestEqual(TEXT("Only one result each"),Results,3);
	const int64 Cancel=QTE->BeginQTE(1,.2f);
	QTE->CancelQTE(Miss); TestTrue(TEXT("Stale cancel cannot close new QTE"),QTE->GetView().bActive);
	QTE->CancelQTE(Cancel); AdvanceBattleTime(World,1.01f);
	TestEqual(TEXT("Cancel has no Miss result"),Results,3);
	TestEqual(TEXT("Cancellation event once"),Cancels,1);
	TestEqual(TEXT("Start event once per session"),Starts,4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionQTEPatternsTest,
	"ExpeditionPortfolio.Combat.QTE.Patterns",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FActionQTEPatternsTest::RunTest(const FString& Parameters)
{
	for (int32 Mode=0; Mode<2; ++Mode)
	for (int32 A=0; A<3; ++A)
	for (int32 B=0; B<3; ++B)
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* QTE=World->GetSubsystem<UActionQTESubsystem>();
		auto* Pattern=NewObject<UActionQTEPatternData>();
		FActionQTEStep Step; Step.Duration=1; Step.PerfectDuration=.2f; Step.DelayAfter=Mode==0?.15f:0;
		Pattern->Steps={Step,Step}; Pattern->Mode=Mode==0?EQTEPatternMode::Sequential:EQTEPatternMode::Rhythm;
		Pattern->SpawnInterval=.6f;
		int32 StepResults=0,FinalResults=0,Starts=0;
		QTE->StepStarted.AddLambda([&](int64,int32) { ++Starts; });
		QTE->StepResolved.AddLambda([&](int64 Id,int32,EAttackTimingResult) { ++StepResults; TestFalse(TEXT("Step callback input reentry blocked"),QTE->SubmitInput(Id)); });
		QTE->Resolved.AddLambda([&](int64,EAttackTimingResult) { ++FinalResults; });
		const int64 Session=QTE->BeginPattern(Pattern);
		TestTrue(TEXT("Two-step pattern starts"),Session>0);
		Pattern->Steps[0].Duration=9; Pattern->PerfectMultiplier=9;
		const int32 Grades[]={A,B};
		if (Mode==1)
		{
			AdvanceBattleTime(World,.65f);
			TestEqual(TEXT("Two independent gauges coexist before first expiry"),QTE->GetView().Gauges.Num(),2);
			TestTrue(TEXT("Independent gauge progress differs"),QTE->GetView().Gauges[0].Progress>QTE->GetView().Gauges[1].Progress);
			for (int32 I=0; I<2; ++I)
			{
				if (Grades[I]==2) { continue; }
				// First Regular is still possible at .65; second Regular at 1.1, Perfect at 1.5.
				const double At=I*.6+(Grades[I]==0?.9:(I==0?.65:.5));
				AdvanceBattleTime(World,FMath::Max(0.f,float(At-World->GetTimeSeconds())));
				TestTrue(TEXT("Overlap accepts one chronological gauge"),QTE->SubmitInput(Session));
				TestFalse(TEXT("Same-time duplicate cannot consume another overlapping gauge"),QTE->SubmitInput(Session));
			}
			AdvanceBattleTime(World,FMath::Max(0.f,float(1.65-World->GetTimeSeconds())));
			const auto Expected=(A==2||B==2)?EAttackTimingResult::Missed:(A==0&&B==0)?EAttackTimingResult::Success:EAttackTimingResult::Failed;
			TestEqual(TEXT("Any overlapping Miss fails whole pattern"),QTE->GetView().OverallResult,Expected);
			TestEqual(TEXT("Overlap aggregate multiplier"),QTE->GetDamageMultiplier(Session),(A==2||B==2)?1.f:1.f+.25f*((A==0)+(B==0)));
			TestFalse(TEXT("Overlap closes after last independent window"),QTE->GetView().bActive);
			TestEqual(TEXT("Overlap step result events once"),StepResults,2);
			TestEqual(TEXT("Overlap aggregate event once"),FinalResults,1);
			TestEqual(TEXT("Overlap spawn events once"),Starts,2);
			continue;
		}
		for (int32 I=0; I<2; ++I)
		{
			if (QTE->Timing.StartedAt>World->GetTimeSeconds())
			{
				TestFalse(TEXT("Gap input rejected"),QTE->SubmitInput(Session));
				AdvanceBattleTime(World,float(QTE->Timing.StartedAt-World->GetTimeSeconds())+.001f);
			}
			TestEqual(TEXT("Ordered current step"),QTE->GetView().StepIndex,I);
			const double Start=QTE->Timing.StartedAt;
			const double InputTime=Start+(Grades[I]==0?.9:Grades[I]==1?.1:1.01);
			AdvanceBattleTime(World,FMath::Max(0.f,float(InputTime-World->GetTimeSeconds())));
			if (Grades[I]!=2)
			{
				TestTrue(TEXT("One input accepted for this step"),QTE->SubmitInput(Session));
				TestFalse(TEXT("Repeated press cannot skip next step"),QTE->SubmitInput(Session));
				AdvanceBattleTime(World,float(Start+1.01-World->GetTimeSeconds()));
			}
		}
		const auto Expected=(A==2||B==2)?EAttackTimingResult::Missed:(A==0&&B==0)?EAttackTimingResult::Success:EAttackTimingResult::Failed;
		TestEqual(TEXT("Any Miss fails the whole pattern"),QTE->GetView().OverallResult,Expected);
		const float Multiplier=(A==2||B==2)?1.f:1.f+.25f*((A==0)+(B==0));
		TestEqual(TEXT("Miss removes every bonus; successful steps average"),QTE->GetDamageMultiplier(Session),Multiplier);
		TestFalse(TEXT("Pattern closes after last window"),QTE->GetView().bActive);
		TestEqual(TEXT("One result per step"),StepResults,2);
		TestEqual(TEXT("One aggregate event"),FinalResults,1);
		TestEqual(TEXT("One start per step including gap"),Starts,2);
	}
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* QTE=World->GetSubsystem<UActionQTESubsystem>();
		auto* Pattern=NewObject<UActionQTEPatternData>();
		TestEqual(TEXT("Empty pattern rejected"),QTE->BeginPattern(Pattern),int64(0));
		FActionQTEStep Step; Step.Duration=.2f; Step.PerfectDuration=.05f;
		Pattern->Steps={Step,Step,Step,Step};
		const int64 Session=QTE->BeginPattern(Pattern);
		AdvanceBattleTime(World,.85f);
		TestEqual(TEXT("Hitch expires all overdue steps"),QTE->GetView().StepIndex,3);
		TestEqual(TEXT("Hitch cannot invent success"),QTE->GetView().OverallResult,EAttackTimingResult::Missed);
		const int64 NewSession=QTE->BeginPattern(Pattern);
		QTE->CancelQTE(Session); TestTrue(TEXT("Stale cancel cannot affect new pattern"),QTE->GetView().bActive);
		QTE->CancelQTE(NewSession); AdvanceBattleTime(World,1);
		TestEqual(TEXT("Cancel cannot turn into timeout Miss"),QTE->GetView().Result,EAttackTimingResult::Cancelled);
		Pattern->Steps[0].PerfectDuration=3;
		TestEqual(TEXT("Invalid step rejected"),QTE->BeginPattern(Pattern),int64(0));
	}
	{
		FScopedBattleTestWorld Scope; auto* World=Scope.GetWorld(); auto* QTE=World->GetSubsystem<UActionQTESubsystem>();
		auto* Pattern=NewObject<UActionQTEPatternData>(); FActionQTEStep Step;
		Step.Duration=1; Step.PerfectDuration=.2f; Pattern->Steps={Step,Step}; Pattern->Mode=EQTEPatternMode::Rhythm; Pattern->SpawnInterval=.18f;
		TestEqual(TEXT("Overlap duration uses last deadline, not sum"),Pattern->GetDuration(),1.18f);
		const int64 Session=QTE->BeginPattern(Pattern);
		AdvanceBattleTime(World,.9f); TestEqual(TEXT("Overlap visible before first press"),QTE->GetView().Gauges.Num(),2);
		TestTrue(TEXT("First Perfect accepted"),QTE->SubmitInput(Session));
		TestEqual(TEXT("Only resolved diamond disappears"),QTE->GetView().Gauges.Num(),1);
		TestEqual(TEXT("Remaining diamond retains its number"),QTE->GetView().Gauges[0].StepIndex,1);
		AdvanceBattleTime(World,.18f); TestTrue(TEXT("Second Perfect .18 seconds later accepted"),QTE->SubmitInput(Session));
		TestEqual(TEXT("Both independently Perfect"),QTE->GetView().OverallResult,EAttackTimingResult::Success);
		AdvanceBattleTime(World,.2f);
		const int64 Cancel=QTE->BeginPattern(Pattern); AdvanceBattleTime(World,.2f); QTE->CancelQTE(Cancel); AdvanceBattleTime(World,2);
		TestEqual(TEXT("Cancel clears both independent gauges"),QTE->GetView().Gauges.Num(),0);
		TestEqual(TEXT("Cancelled overlap never times out"),QTE->GetView().Result,EAttackTimingResult::Cancelled);
		QTE->BeginPattern(Pattern); AdvanceBattleTime(World,2);
		TestEqual(TEXT("Overlap hitch misses all pending gauges"),QTE->GetView().OverallResult,EAttackTimingResult::Missed);
		Pattern->SpawnInterval=0; TestEqual(TEXT("Zero spawn interval rejected"),QTE->BeginPattern(Pattern),int64(0));
		Pattern->SpawnInterval=.18f; Pattern->Steps[1].Duration=.1f; Pattern->Steps[1].PerfectDuration=.05f;
		TestEqual(TEXT("Reversed gauge deadlines rejected"),QTE->BeginPattern(Pattern),int64(0));
	}
	return true;
}

#endif
