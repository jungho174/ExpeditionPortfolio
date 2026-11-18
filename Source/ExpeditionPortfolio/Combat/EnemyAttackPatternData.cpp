#include "Combat/EnemyAttackPatternData.h"
#include "Animation/AnimMontage.h"
#include "UI/AnimNotify_EnemyHit.h"

float UEnemyAttackPatternData::GetHitTime() const
{
	if (!Hits.IsEmpty()) { return ResolveHits()[0].HitTime; }
	if (!AttackMontage) { return DefenseWindowDuration; }
	float BaseTime = FallbackHitTime;
	for (const FAnimNotifyEvent& Event : AttackMontage->Notifies)
	{
		if (Event.Notify && Event.Notify->IsA<UAnimNotify_EnemyHit>())
		{
			BaseTime = Event.GetTriggerTime();
			break;
		}
	}
	return BaseTime + HitTimeOffset;
}

TArray<FEnemyAttackHit> UEnemyAttackPatternData::ResolveHits() const
{
	TArray<FEnemyAttackHit> Result = Hits;
	if (Result.IsEmpty())
	{
		FEnemyAttackHit Hit;
		Hit.Damage=Damage; Hit.DefenseWindowDuration=DefenseWindowDuration;
		Hit.ParrySuccessDuration=ParrySuccessDuration; Hit.HitTimeOffset=HitTimeOffset;
		Hit.FallbackHitTime=FallbackHitTime; Hit.HitTime=GetHitTime();
		Result.Add(Hit);
		return Result;
	}
	for (int32 Index=0; Index<Result.Num(); ++Index)
	{
		float Time=Result[Index].FallbackHitTime;
		if (AttackMontage)
		{
			for (const FAnimNotifyEvent& Event:AttackMontage->Notifies)
			{
				const auto* Notify=Cast<UAnimNotify_EnemyHit>(Event.Notify);
				if (Notify && Notify->HitIndex==Index) { Time=Event.GetTriggerTime(); break; }
			}
		}
		Result[Index].HitTime=Time+Result[Index].HitTimeOffset;
	}
	return Result;
}

bool UEnemyAttackPatternData::ValidatePattern(FString& OutReason) const
{
	OutReason.Reset();
	if (!Hits.IsEmpty())
	{
		if (PatternId.IsNone() || Hits.Num()>32 || !FMath::IsFinite(TelegraphDuration) || TelegraphDuration<=0
			|| !FMath::IsFinite(SelectionWeight) || SelectionWeight<0 || (AttackMontage && AttackMontage->RateScale!=1))
		{ OutReason=TEXT("Invalid multi-hit pattern header, count or montage rate"); return false; }
		const auto Resolved=ResolveHits();
		float PreviousTime=0;
		for (int32 Index=0; Index<Resolved.Num(); ++Index)
		{
			const auto& Hit=Resolved[Index];
			int32 MarkerCount=0;
			if (AttackMontage) { for (const auto& Event:AttackMontage->Notifies) { const auto* Notify=Cast<UAnimNotify_EnemyHit>(Event.Notify); if (Notify && Notify->HitIndex==Index) { ++MarkerCount; } } }
			if (MarkerCount>1 || !FMath::IsFinite(Hit.Damage) || Hit.Damage<=0
				|| !FMath::IsFinite(Hit.DefenseWindowDuration) || Hit.DefenseWindowDuration<=0
				|| !FMath::IsFinite(Hit.ParrySuccessDuration) || Hit.ParrySuccessDuration<=0 || Hit.ParrySuccessDuration>Hit.DefenseWindowDuration
				|| !FMath::IsFinite(Hit.HitTimeOffset) || !FMath::IsFinite(Hit.FallbackHitTime) || Hit.FallbackHitTime<=0
				|| !FMath::IsFinite(Hit.HitTime) || Hit.HitTime-Hit.DefenseWindowDuration<PreviousTime-KINDA_SMALL_NUMBER
				|| Hit.HitTime<=PreviousTime || (AttackMontage && Hit.HitTime>=AttackMontage->GetPlayLength())
				|| !FMath::IsFinite(Hit.SoundVolume) || Hit.SoundVolume<0 || !FMath::IsFinite(Hit.SoundDelay) || Hit.SoundDelay<0
				|| !FMath::IsFinite(Hit.SoundStartTime) || Hit.SoundStartTime<0 || !FMath::IsFinite(Hit.EffectScale) || Hit.EffectScale<0)
			{ OutReason=FString::Printf(TEXT("Hit %d: invalid timing/damage/feedback, duplicate marker, or overlapping defense windows"),Index+1); return false; }
			PreviousTime=Hit.HitTime;
		}
		return true;
	}
	if (PatternId.IsNone())
	{
		OutReason = TEXT("PatternId is empty");
	}
	else if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		OutReason = TEXT("Damage must be finite and positive");
	}
	else if (!FMath::IsFinite(TelegraphDuration) || TelegraphDuration <= 0.0f
		|| !FMath::IsFinite(DefenseWindowDuration) || DefenseWindowDuration <= 0.0f)
	{
		OutReason = TEXT("Telegraph and defense durations must be finite and positive");
	}
	else if (!FMath::IsFinite(ParrySuccessDuration) || ParrySuccessDuration <= 0.0f
		|| ParrySuccessDuration > DefenseWindowDuration)
	{
		OutReason = TEXT("Parry duration must be positive and within the defense window");
	}
	else if (!FMath::IsFinite(SelectionWeight) || SelectionWeight < 0.0f)
	{
		OutReason = TEXT("SelectionWeight must be finite and non-negative");
	}
	else if (!FMath::IsFinite(HitTimeOffset) || !FMath::IsFinite(FallbackHitTime) || FallbackHitTime <= 0.0f)
	{
		OutReason = TEXT("Hit offset must be finite; fallback hit time must be finite and positive");
	}
	else if (AttackMontage && (!FMath::IsFinite(GetHitTime()) || GetHitTime() < DefenseWindowDuration
		|| GetHitTime() >= AttackMontage->GetPlayLength() || AttackMontage->RateScale != 1.0f))
	{
		OutReason = TEXT("Adjusted hit must fit the full defense window and precede montage end; montage RateScale must be 1");
	}
	return OutReason.IsEmpty();
}

FEnemyAttackPatternSnapshot UEnemyAttackPatternData::MakeSnapshot() const
{
	FEnemyAttackPatternSnapshot Snapshot;
	Snapshot.PatternId = PatternId;
	Snapshot.DisplayName = DisplayName;
	Snapshot.Damage = Damage;
	Snapshot.TelegraphDuration = TelegraphDuration;
	Snapshot.DefenseWindowDuration = DefenseWindowDuration;
	Snapshot.ParrySuccessDuration = ParrySuccessDuration;
	Snapshot.AttackMontage = AttackMontage;
	Snapshot.HitTime = GetHitTime();
	Snapshot.HitTimeOffset = HitTimeOffset;
	Snapshot.Hits = ResolveHits();
	if (!Hits.IsEmpty())
	{
		Snapshot.Damage=Snapshot.Hits[0].Damage;
		Snapshot.DefenseWindowDuration=Snapshot.Hits[0].DefenseWindowDuration;
		Snapshot.ParrySuccessDuration=Snapshot.Hits[0].ParrySuccessDuration;
		Snapshot.HitTimeOffset=Snapshot.Hits[0].HitTimeOffset;
	}
	return Snapshot;
}

const UEnemyAttackPatternData* UEnemyAttackPatternData::SelectPattern(
	const TArray<TObjectPtr<UEnemyAttackPatternData>>& Patterns, FRandomStream& Random)
{
	double TotalWeight = 0.0;
	const UEnemyAttackPatternData* LastEligible = nullptr;
	for (const UEnemyAttackPatternData* Pattern : Patterns)
	{
		FString Reason;
		if (IsValid(Pattern) && Pattern->ValidatePattern(Reason) && Pattern->SelectionWeight > 0.0f)
		{
			TotalWeight += static_cast<double>(Pattern->SelectionWeight);
			LastEligible = Pattern;
		}
	}
	if (!LastEligible)
	{
		return nullptr;
	}
	double Remaining = static_cast<double>(Random.GetFraction()) * TotalWeight;
	for (const UEnemyAttackPatternData* Pattern : Patterns)
	{
		FString Reason;
		if (IsValid(Pattern) && Pattern->ValidatePattern(Reason) && Pattern->SelectionWeight > 0.0f)
		{
			Remaining -= static_cast<double>(Pattern->SelectionWeight);
			if (Remaining < 0.0)
			{
				return Pattern;
			}
		}
	}
	return LastEligible;
}
