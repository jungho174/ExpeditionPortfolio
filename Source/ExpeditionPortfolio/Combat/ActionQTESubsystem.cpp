#include "Combat/ActionQTESubsystem.h"
#include "Engine/World.h"

int64 UActionQTESubsystem::BeginQTE(float Duration, float PerfectDuration, float Multiplier)
{
	FActionQTEStep Step; Step.Duration=Duration; Step.PerfectDuration=PerfectDuration;
	return BeginSteps({Step}, EQTEPatternMode::Sequential, Multiplier);
}
int64 UActionQTESubsystem::BeginPattern(const UActionQTEPatternData* Pattern)
{
	if (!Pattern || !Pattern->IsValidConfiguration()) { return 0; }
	if (Pattern->Mode==EQTEPatternMode::Rhythm)
	{
		if (bDispatching || bPlaying) { return 0; }
		Steps=Pattern->Steps; Mode=Pattern->Mode; PerfectMultiplier=Pattern->PerfectMultiplier; PatternDuration=Pattern->GetDuration();
		PatternStartedAt=GetWorld()->GetTimeSeconds(); Session=++Serial;
		Results.Init(EAttackTimingResult::Pending,Steps.Num()); RhythmAnnounced.Init(false,Steps.Num()); RhythmWindows.Reset();
		bPlaying=true; bFinalResolved=false; LastRhythmInputAt=-1; StepIndex=0;
		for (int32 I=0; I<Steps.Num(); ++I)
		{
			FAttackTimingWindow Window;
			Window.Begin(PatternStartedAt+I*Pattern->SpawnInterval,Steps[I].Duration,Steps[I].PerfectDuration,PerfectMultiplier);
			RhythmWindows.Add(Window);
		}
		Timing=RhythmWindows[0]; RhythmAnnounced[0]=true; ScheduleRhythmBoundary();
		TGuardValue<bool> Guard(bDispatching,true);
		Started.Broadcast(Session); StepStarted.Broadcast(Session,0); Changed.Broadcast();
		return Session;
	}
	return BeginSteps(Pattern->Steps, Pattern->Mode, Pattern->PerfectMultiplier);
}
void UActionQTESubsystem::UpdateRhythmFocus()
{
	StepIndex=RhythmWindows.Num()-1;
	for (int32 I=0; I<Results.Num(); ++I)
	{
		if (Results[I]==EAttackTimingResult::Pending) { StepIndex=I; break; }
	}
	Timing=RhythmWindows[StepIndex];
}
void UActionQTESubsystem::RecordRhythmStep(int32 Index)
{
	if (Results[Index]!=EAttackTimingResult::Pending) { return; }
	Results[Index]=RhythmWindows[Index].Result;
	StepResolved.Broadcast(Session,Index,Results[Index]);
	if (!bFinalResolved && !Results.Contains(EAttackTimingResult::Pending))
	{
		bFinalResolved=true; Resolved.Broadcast(Session,OverallResult());
	}
}
void UActionQTESubsystem::ScheduleRhythmBoundary()
{
	const double Now=GetWorld()->GetTimeSeconds();
	double Next=PatternStartedAt+PatternDuration;
	for (int32 I=0; I<RhythmWindows.Num(); ++I)
	{
		const auto& Window=RhythmWindows[I];
		if (!RhythmAnnounced[I]) { Next=FMath::Min(Next,Window.StartedAt); }
		if (Window.bOpen) { Next=FMath::Min(Next,Window.StartedAt+Window.Duration); }
	}
	GetWorld()->GetTimerManager().SetTimer(EndTimer,FTimerDelegate::CreateUObject(this,&ThisClass::AdvanceRhythm,Session),FMath::Max(.001f,float(Next-Now)),false);
}
void UActionQTESubsystem::AdvanceRhythm(int64 ExpectedSession)
{
	if (bDispatching || !bPlaying || ExpectedSession!=Session) { return; }
	TGuardValue<bool> Guard(bDispatching,true);
	const double Now=GetWorld()->GetTimeSeconds();
	for (int32 I=0; I<RhythmWindows.Num(); ++I)
	{
		auto& Window=RhythmWindows[I];
		if (!RhythmAnnounced[I] && Now>=Window.StartedAt-1.e-6)
		{
			RhythmAnnounced[I]=true; StepStarted.Broadcast(Session,I);
		}
		if (Window.bOpen && Now>=Window.StartedAt+Window.Duration-1.e-6) { Window.Close(); RecordRhythmStep(I); }
	}
	UpdateRhythmFocus();
	bPlaying=Now<PatternStartedAt+PatternDuration-1.e-6;
	if (bPlaying) { ScheduleRhythmBoundary(); } else { GetWorld()->GetTimerManager().ClearTimer(EndTimer); }
	Changed.Broadcast();
}
int64 UActionQTESubsystem::BeginSteps(const TArray<FActionQTEStep>& NewSteps, EQTEPatternMode NewMode, float Multiplier)
{
	if (bDispatching || bPlaying || NewSteps.IsEmpty() || NewSteps.Num()>16 || !FMath::IsFinite(Multiplier) || Multiplier<1 || Multiplier>10) { return 0; }
	float Total=0;
	for (int32 I=0; I<NewSteps.Num(); ++I)
	{
		if (!NewSteps[I].IsValid()) { return 0; }
		Total+=NewSteps[I].Duration+(I+1<NewSteps.Num()?NewSteps[I].DelayAfter:0);
	}
	if (Total>45) { return 0; }
	Steps=NewSteps; Mode=NewMode; PerfectMultiplier=Multiplier; PatternDuration=Total;
	RhythmWindows.Reset(); RhythmAnnounced.Reset();
	Results.Init(EAttackTimingResult::Pending,Steps.Num()); StepIndex=0;
	Session=++Serial; bPlaying=true; bFinalResolved=false; PatternStartedAt=GetWorld()->GetTimeSeconds();
	StartStep(PatternStartedAt);
	TGuardValue<bool> Guard(bDispatching,true);
	Started.Broadcast(Session); StepStarted.Broadcast(Session,0); Changed.Broadcast();
	return Session;
}
void UActionQTESubsystem::StartStep(double Time)
{
	Timing.Begin(Time,Steps[StepIndex].Duration,Steps[StepIndex].PerfectDuration,PerfectMultiplier);
	bStepAnnounced=GetWorld()->GetTimeSeconds()>=Time-1.e-6;
	ScheduleBoundary();
}
void UActionQTESubsystem::ScheduleBoundary()
{
	const double Now=GetWorld()->GetTimeSeconds();
	const double Boundary=Now<Timing.StartedAt ? Timing.StartedAt : Timing.StartedAt+Timing.Duration;
	GetWorld()->GetTimerManager().SetTimer(EndTimer,FTimerDelegate::CreateUObject(this,&ThisClass::AdvanceSteps,Session),FMath::Max(.001f,float(Boundary-Now)),false);
}
EAttackTimingResult UActionQTESubsystem::OverallResult() const
{
	if (!Session) { return EAttackTimingResult::None; }
	if (Timing.Result==EAttackTimingResult::Cancelled) { return EAttackTimingResult::Cancelled; }
	bool bPending=false,bRegular=false;
	for (auto Result:Results)
	{
		if (Result==EAttackTimingResult::Missed) { return EAttackTimingResult::Missed; }
		bPending|=Result==EAttackTimingResult::Pending; bRegular|=Result==EAttackTimingResult::Failed;
	}
	return bPending ? EAttackTimingResult::Pending : bRegular ? EAttackTimingResult::Failed : EAttackTimingResult::Success;
}
void UActionQTESubsystem::RecordStep()
{
	if (Results[StepIndex]!=EAttackTimingResult::Pending) { return; }
	Results[StepIndex]=Timing.Result;
	StepResolved.Broadcast(Session,StepIndex,Timing.Result);
	if (StepIndex+1==Steps.Num() && !bFinalResolved)
	{
		bFinalResolved=true; Resolved.Broadcast(Session,OverallResult());
	}
}
void UActionQTESubsystem::AdvanceSteps(int64 ExpectedSession)
{
	if (bDispatching || !bPlaying || ExpectedSession!=Session) { return; }
	TGuardValue<bool> Guard(bDispatching,true);
	const double Now=GetWorld()->GetTimeSeconds();
	while (bPlaying && Now>=Timing.StartedAt+Timing.Duration-1.e-6)
	{
		const double NextStart=Timing.StartedAt+Timing.Duration+Steps[StepIndex].DelayAfter;
		Timing.Close(); RecordStep();
		if (StepIndex+1==Steps.Num()) { bPlaying=false; break; }
		++StepIndex; StartStep(NextStart);
		if (bStepAnnounced) { StepStarted.Broadcast(Session,StepIndex); }
	}
	if (bPlaying && !bStepAnnounced && Now>=Timing.StartedAt-1.e-6)
	{
		bStepAnnounced=true; StepStarted.Broadcast(Session,StepIndex);
	}
	if (bPlaying) { ScheduleBoundary(); }
	else { GetWorld()->GetTimerManager().ClearTimer(EndTimer); }
	Changed.Broadcast();
}
bool UActionQTESubsystem::SubmitInput(int64 ExpectedSession)
{
	if (bDispatching || !Session || Session!=ExpectedSession || !bPlaying) { return false; }
	if (Mode==EQTEPatternMode::Rhythm)
	{
		AdvanceRhythm(ExpectedSession);
		const double Now=GetWorld()->GetTimeSeconds();
		if (!bPlaying || Now<=LastRhythmInputAt+1.e-6 || !RhythmWindows[StepIndex].Submit(Now)) { return false; }
		LastRhythmInputAt=Now;
		TGuardValue<bool> Guard(bDispatching,true);
		RecordRhythmStep(StepIndex); UpdateRhythmFocus(); Changed.Broadcast(); return true;
	}
	AdvanceSteps(ExpectedSession);
	if (!bPlaying || !Timing.Submit(GetWorld()->GetTimeSeconds())) { return false; }
	TGuardValue<bool> Guard(bDispatching,true);
	RecordStep(); Changed.Broadcast(); return true;
}
void UActionQTESubsystem::FinishWindow(int64 ExpectedSession)
{
	if (bDispatching || !Session || Session!=ExpectedSession || !bPlaying) { return; }
	if (Mode==EQTEPatternMode::Rhythm)
	{
		GetWorld()->GetTimerManager().ClearTimer(EndTimer);
		TGuardValue<bool> Guard(bDispatching,true);
		for (int32 I=0; I<RhythmWindows.Num(); ++I) { RhythmWindows[I].Close(); RecordRhythmStep(I); }
		UpdateRhythmFocus(); bPlaying=false; Changed.Broadcast(); return;
	}
	GetWorld()->GetTimerManager().ClearTimer(EndTimer);
	TGuardValue<bool> Guard(bDispatching,true);
	for (;;)
	{
		Timing.Close(); RecordStep();
		if (StepIndex+1==Steps.Num()) { break; }
		++StepIndex; Timing.Begin(GetWorld()->GetTimeSeconds(),Steps[StepIndex].Duration,Steps[StepIndex].PerfectDuration,PerfectMultiplier);
	}
	bPlaying=false; Changed.Broadcast();
}
void UActionQTESubsystem::CancelQTE(int64 ExpectedSession)
{
	if (bDispatching || !Session || Session!=ExpectedSession || Timing.Result==EAttackTimingResult::Cancelled) { return; }
	GetWorld()->GetTimerManager().ClearTimer(EndTimer);
	Timing.Close(true); bPlaying=false;
	TGuardValue<bool> Guard(bDispatching,true);
	Cancelled.Broadcast(Session); Changed.Broadcast();
}
FAttackTimingView UActionQTESubsystem::GetView() const
{
	FAttackTimingView View; const double Now=GetWorld()->GetTimeSeconds();
	View.SessionId=Session; View.bActive=bPlaying; View.Result=bPlaying ? Timing.Result : OverallResult();
	View.StepIndex=StepIndex; View.StepCount=Steps.Num(); View.Mode=Mode; View.OverallResult=OverallResult();
	View.DamageMultiplier=GetDamageMultiplier(Session);
	if (Timing.Duration>0)
	{
		View.StepProgress=FMath::Clamp(float(Now-Timing.StartedAt)/Timing.Duration,0.f,1.f);
		View.StepSuccessStart=1-Timing.SuccessDuration/Timing.Duration;
		View.bWaitingBetweenSteps=bPlaying && Now<Timing.StartedAt;
		View.Progress=bPlaying ? View.StepProgress : 1; View.SuccessStart=View.StepSuccessStart;
		if (Mode==EQTEPatternMode::Rhythm && PatternDuration>0)
		{
			for (int32 I=0; bPlaying && I<RhythmWindows.Num(); ++I)
			{
				const auto& Window=RhythmWindows[I];
				if (Results[I]!=EAttackTimingResult::Pending || Now<Window.StartedAt || Now>=Window.StartedAt+Window.Duration) { continue; }
				FQTEGaugeView Gauge; Gauge.StepIndex=I;
				Gauge.Progress=FMath::Clamp(float(Now-Window.StartedAt)/Window.Duration,0.f,1.f);
				Gauge.SuccessStart=1-Window.SuccessDuration/Window.Duration;
				View.Gauges.Add(Gauge);
			}
			View.PerfectWindows.Add(FVector2D(View.SuccessStart,1));
		}
		else { View.PerfectWindows.Add(FVector2D(View.SuccessStart,1)); }
	}
	return View;
}
float UActionQTESubsystem::GetDamageMultiplier(int64 ExpectedSession) const
{
	if (!Session || Session!=ExpectedSession || Results.IsEmpty() || OverallResult()==EAttackTimingResult::Cancelled) { return 1; }
	float Sum=0;
	for (auto Result:Results)
	{
		if (Result==EAttackTimingResult::Missed || Result==EAttackTimingResult::Pending) { return 1; }
		Sum+=Result==EAttackTimingResult::Success ? PerfectMultiplier : 1;
	}
	return Sum/Results.Num();
}
void UActionQTESubsystem::Deinitialize()
{
	GetWorld()->GetTimerManager().ClearTimer(EndTimer);
	Started.Clear(); Resolved.Clear(); Cancelled.Clear(); Changed.Clear(); StepStarted.Clear(); StepResolved.Clear();
	Timing.Close(true); bPlaying=false; Session=0; Results.Reset(); Steps.Reset();
	Super::Deinitialize();
}
