#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/AttackTiming.h"
#include "TimerManager.h"
#include "ActionQTESubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnQTEStarted, int64);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnQTEResolved, int64, EAttackTimingResult);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnQTEStepStarted, int64, int32);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnQTEStepResolved, int64, int32, EAttackTimingResult);

// One active QTE per world; independent of spell, montage, battle state, widget and sound.
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UActionQTESubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Combat|QTE") int64 BeginQTE(float Duration, float PerfectDuration, float PerfectMultiplier = 1.5f);
	UFUNCTION(BlueprintCallable, Category="Combat|QTE") int64 BeginPattern(const UActionQTEPatternData* Pattern);
	UFUNCTION(BlueprintCallable, Category="Combat|QTE") bool SubmitInput(int64 ExpectedSession);
	UFUNCTION(BlueprintCallable, Category="Combat|QTE") void FinishWindow(int64 ExpectedSession);
	UFUNCTION(BlueprintCallable, Category="Combat|QTE") void CancelQTE(int64 ExpectedSession);
	UFUNCTION(BlueprintPure, Category="Combat|QTE") FAttackTimingView GetView() const;
	float GetDamageMultiplier(int64 ExpectedSession) const;
	FSimpleMulticastDelegate Changed;
	FOnQTEStarted Started;
	FOnQTEResolved Resolved;
	FOnQTEStarted Cancelled;
	FOnQTEStepStarted StepStarted;
	FOnQTEStepResolved StepResolved;
	virtual void Deinitialize() override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FAttackTimingIntegrationTest;
	friend class FActionQTESessionsTest;
	friend class FActionQTEPatternsTest;
#endif
	FAttackTimingWindow Timing;
	FTimerHandle EndTimer;
	int64 Serial = 0, Session = 0;
	bool bDispatching = false;
	bool bPlaying = false, bFinalResolved = false;
	bool bStepAnnounced = false;
	TArray<FAttackTimingWindow> RhythmWindows;
	TArray<bool> RhythmAnnounced;
	double LastRhythmInputAt = -1;
	void ScheduleRhythmBoundary();
	void AdvanceRhythm(int64 ExpectedSession);
	void RecordRhythmStep(int32 Index);
	void UpdateRhythmFocus();
	int32 StepIndex = 0;
	EQTEPatternMode Mode = EQTEPatternMode::Sequential;
	TArray<FActionQTEStep> Steps;
	TArray<EAttackTimingResult> Results;
	double PatternStartedAt = 0;
	float PatternDuration = 0, PerfectMultiplier = 1.5f;
	int64 BeginSteps(const TArray<FActionQTEStep>& NewSteps, EQTEPatternMode NewMode, float Multiplier);
	void StartStep(double Time);
	void ScheduleBoundary();
	void AdvanceSteps(int64 ExpectedSession);
	void RecordStep();
	EAttackTimingResult OverallResult() const;
};
