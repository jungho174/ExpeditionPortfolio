#pragma once

#include "CoreMinimal.h"
#include "Combat/ActionQTEPatternData.h"
#include "AttackTiming.generated.h"

UENUM(BlueprintType)
// Preserve serialized enum names/values. Success/Failed now mean Perfect/Regular.
enum class EAttackTimingResult : uint8
{
	None, Pending, Success UMETA(DisplayName="Perfect"), Failed UMETA(DisplayName="Regular"),
	Missed UMETA(DisplayName="Miss"), Cancelled
};

UENUM(BlueprintType)
enum class EPlayerActionPhase : uint8 { None, Preparing, Executing };

USTRUCT(BlueprintType)
struct FQTEGaugeView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 StepIndex = 0;
	UPROPERTY(BlueprintReadOnly) float Progress = 0;
	UPROPERTY(BlueprintReadOnly) float SuccessStart = 1;
};

USTRUCT(BlueprintType)
struct FAttackTimingView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bActive = false;
	UPROPERTY(BlueprintReadOnly) float Progress = 0;
	UPROPERTY(BlueprintReadOnly) float SuccessStart = 1;
	UPROPERTY(BlueprintReadOnly) EAttackTimingResult Result = EAttackTimingResult::None;
	UPROPERTY(BlueprintReadOnly) int64 SessionId = 0;
	UPROPERTY(BlueprintReadOnly) int32 StepIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 StepCount = 1;
	UPROPERTY(BlueprintReadOnly) float StepProgress = 0;
	UPROPERTY(BlueprintReadOnly) float StepSuccessStart = 1;
	UPROPERTY(BlueprintReadOnly) float DamageMultiplier = 1;
	UPROPERTY(BlueprintReadOnly) EAttackTimingResult OverallResult = EAttackTimingResult::None;
	UPROPERTY(BlueprintReadOnly) bool bWaitingBetweenSteps = false;
	UPROPERTY(BlueprintReadOnly) EQTEPatternMode Mode = EQTEPatternMode::Sequential;
	UPROPERTY(BlueprintReadOnly) TArray<FVector2D> PerfectWindows;
	UPROPERTY(BlueprintReadOnly) TArray<FQTEGaugeView> Gauges;
};

// One clock and one attempt per action. The widget only displays this state.
struct FAttackTimingWindow
{
	double StartedAt = 0;
	float Duration = 0;
	float SuccessDuration = 0;
	float Multiplier = 1;
	bool bOpen = false;
	EAttackTimingResult Result = EAttackTimingResult::None;
	bool Begin(double Now, float Length, float Window, float Bonus)
	{
		if (!FMath::IsFinite(Now) || !FMath::IsFinite(Length) || !FMath::IsFinite(Window) || !FMath::IsFinite(Bonus)
			|| Length <= 0 || Window <= 0 || Window > Length || Bonus < 1 || Bonus > 10) { return false; }
		StartedAt = Now; Duration = Length; SuccessDuration = Window; Multiplier = Bonus;
		bOpen = true; Result = EAttackTimingResult::Pending; return true;
	}
	bool Submit(double Now)
	{
		const double Elapsed = Now - StartedAt;
		if (!bOpen || Result != EAttackTimingResult::Pending || !FMath::IsFinite(Elapsed) || Elapsed < 0 || Elapsed >= Duration) { return false; }
		Result = Elapsed >= double(Duration - SuccessDuration) - 1.e-6 ? EAttackTimingResult::Success : EAttackTimingResult::Failed;
		return true;
	}
	void Close(bool bCancelled = false)
	{
		bOpen = false;
		if (bCancelled) { Result = EAttackTimingResult::Cancelled; }
		else if (Result == EAttackTimingResult::Pending) { Result = EAttackTimingResult::Missed; }
	}
	float DamageMultiplier() const { return Result == EAttackTimingResult::Success ? Multiplier : 1; }
};
