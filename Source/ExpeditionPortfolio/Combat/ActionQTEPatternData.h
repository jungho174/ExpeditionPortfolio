#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ActionQTEPatternData.generated.h"

UENUM(BlueprintType)
enum class EQTEPatternMode : uint8 { Sequential, Rhythm };

USTRUCT(BlueprintType)
struct FActionQTEStep
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.05",ClampMax="10",Units="s")) float Duration = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01",Units="s")) float PerfectDuration = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="5",Units="s")) float DelayAfter = 0;
	bool IsValid() const
	{
		return FMath::IsFinite(Duration) && Duration >= .05f && Duration <= 10
			&& FMath::IsFinite(PerfectDuration) && PerfectDuration >= .01f && PerfectDuration <= Duration
			&& FMath::IsFinite(DelayAfter) && DelayAfter >= 0 && DelayAfter <= 5;
	}
};

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UActionQTEPatternData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="QTE") EQTEPatternMode Mode = EQTEPatternMode::Sequential;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="QTE") TArray<FActionQTEStep> Steps;
	// Rhythm: time between spawning successive independent gauges. Sequential uses DelayAfter instead.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="QTE", meta=(EditCondition="Mode == EQTEPatternMode::Rhythm",ClampMin="0.01",ClampMax="5",Units="s")) float SpawnInterval = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="QTE", meta=(ClampMin="1",ClampMax="10")) float PerfectMultiplier = 1.5f;
	float GetDuration() const
	{
		float Total = 0;
		for (int32 I=0; I<Steps.Num(); ++I)
		{
			if (Mode==EQTEPatternMode::Rhythm) { Total=FMath::Max(Total,I*SpawnInterval+Steps[I].Duration); }
			else { Total += Steps[I].Duration + (I+1<Steps.Num() ? Steps[I].DelayAfter : 0); }
		}
		return Total;
	}
	bool IsValidConfiguration() const
	{
		if (Steps.IsEmpty() || Steps.Num()>16 || !FMath::IsFinite(PerfectMultiplier) || PerfectMultiplier<1 || PerfectMultiplier>10) { return false; }
		for (const auto& Step : Steps) { if (!Step.IsValid()) { return false; } }
		if (Mode==EQTEPatternMode::Rhythm)
		{
			if (!FMath::IsFinite(SpawnInterval) || SpawnInterval<.01f || SpawnInterval>5) { return false; }
			// Input order follows the gauge numbers; reject equal/reversed deadlines.
			for (int32 I=1; I<Steps.Num(); ++I)
			{
				if (I*SpawnInterval+Steps[I].Duration <= (I-1)*SpawnInterval+Steps[I-1].Duration+.001f) { return false; }
			}
		}
		return FMath::IsFinite(GetDuration()) && GetDuration()<=45;
	}
};
