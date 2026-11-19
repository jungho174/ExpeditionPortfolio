#include "Combat/EnemyAttackPatternData.h"
#include "Animation/AnimMontage.h"
#include "UI/AnimNotify_EnemyHit.h"

float UEnemyAttackPatternData::GetHitTime() const
{
	return ResolveHits()[0].HitTime;
}

TArray<FEnemyAttackHit> UEnemyAttackPatternData::ResolveHits() const
{
	TArray<int32> MarkerCounts;
	return ResolveHits(MarkerCounts);
}

TArray<FEnemyAttackHit> UEnemyAttackPatternData::ResolveHits(TArray<int32>& OutMarkerCounts) const
{
	if (Hits.IsEmpty())
	{
		// 기존 단타는 첫 타격 노티파이를 쓰고, 몽타주가 없으면 방어 창 끝에 명중한다.
		FEnemyAttackHit Hit;
		Hit.Damage = Damage;
		Hit.DefenseWindowDuration = DefenseWindowDuration;
		Hit.ParrySuccessDuration = ParrySuccessDuration;
		Hit.HitTimeOffset = HitTimeOffset;
		Hit.FallbackHitTime = FallbackHitTime;
		Hit.HitTime = DefenseWindowDuration;
		if (AttackMontage)
		{
			float BaseTime = FallbackHitTime;
			for (const FAnimNotifyEvent& Event : AttackMontage->Notifies)
			{
				if (Event.Notify && Event.Notify->IsA<UAnimNotify_EnemyHit>())
				{
					BaseTime = Event.GetTriggerTime();
					break;
				}
			}
			Hit.HitTime = BaseTime + HitTimeOffset;
		}
		return {Hit};
	}

	TArray<FEnemyAttackHit> Resolved = Hits;
	OutMarkerCounts.Init(0, Resolved.Num());
	for (FEnemyAttackHit& Hit : Resolved)
	{
		Hit.HitTime = Hit.FallbackHitTime + Hit.HitTimeOffset;
	}
	if (AttackMontage)
	{
		for (const FAnimNotifyEvent& Event : AttackMontage->Notifies)
		{
			const auto* Notify = Cast<UAnimNotify_EnemyHit>(Event.Notify);
			if (!Notify || !Resolved.IsValidIndex(Notify->HitIndex)) { continue; }

			const int32 Index = Notify->HitIndex;
			if (++OutMarkerCounts[Index] == 1)
			{
				Resolved[Index].HitTime = Event.GetTriggerTime() + Resolved[Index].HitTimeOffset;
			}
		}
	}
	return Resolved;
}

bool UEnemyAttackPatternData::ValidatePattern(FString& OutReason) const
{
	TArray<FEnemyAttackHit> Resolved;
	return TryResolveValidHits(Resolved, OutReason);
}

bool UEnemyAttackPatternData::TryResolveValidHits(TArray<FEnemyAttackHit>& OutHits, FString& OutReason) const
{
	OutReason.Reset();
	if (!Hits.IsEmpty() && (PatternId.IsNone() || Hits.Num() > 32 || !FMath::IsFinite(TelegraphDuration) || TelegraphDuration <= 0
		|| !FMath::IsFinite(SelectionWeight) || SelectionWeight < 0 || (AttackMontage && AttackMontage->RateScale != 1)))
	{
		OutReason = TEXT("Invalid multi-hit pattern header, count or montage rate");
		return false;
	}

	TArray<int32> MarkerCounts;
	OutHits = ResolveHits(MarkerCounts);
	if (!Hits.IsEmpty())
	{
		// 타격마다 방어 창을 따로 열 수 있도록 중복 표식과 겹치는 구간을 검사한다.
		float PreviousTime = 0.0f;
		for (int32 Index = 0; Index < OutHits.Num(); ++Index)
		{
			const FEnemyAttackHit& Hit = OutHits[Index];
			if (MarkerCounts[Index] > 1 || !FMath::IsFinite(Hit.Damage) || Hit.Damage <= 0
				|| !FMath::IsFinite(Hit.DefenseWindowDuration) || Hit.DefenseWindowDuration <= 0
				|| !FMath::IsFinite(Hit.ParrySuccessDuration) || Hit.ParrySuccessDuration <= 0 || Hit.ParrySuccessDuration > Hit.DefenseWindowDuration
				|| !FMath::IsFinite(Hit.HitTimeOffset) || !FMath::IsFinite(Hit.FallbackHitTime) || Hit.FallbackHitTime <= 0
				|| !FMath::IsFinite(Hit.HitTime) || Hit.HitTime - Hit.DefenseWindowDuration < PreviousTime - KINDA_SMALL_NUMBER
				|| Hit.HitTime <= PreviousTime || (AttackMontage && Hit.HitTime >= AttackMontage->GetPlayLength())
				|| !FMath::IsFinite(Hit.SoundVolume) || Hit.SoundVolume < 0 || !FMath::IsFinite(Hit.SoundDelay) || Hit.SoundDelay < 0
				|| !FMath::IsFinite(Hit.SoundStartTime) || Hit.SoundStartTime < 0 || !FMath::IsFinite(Hit.EffectScale) || Hit.EffectScale < 0)
			{
				OutReason = FString::Printf(TEXT("Hit %d: invalid timing/damage/feedback, duplicate marker, or overlapping defense windows"), Index + 1);
				return false;
			}
			PreviousTime = Hit.HitTime;
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
	else if (AttackMontage && (!FMath::IsFinite(OutHits[0].HitTime) || OutHits[0].HitTime < DefenseWindowDuration
		|| OutHits[0].HitTime >= AttackMontage->GetPlayLength() || AttackMontage->RateScale != 1.0f))
	{
		OutReason = TEXT("Adjusted hit must fit the full defense window and precede montage end; montage RateScale must be 1");
	}
	return OutReason.IsEmpty();
}

FEnemyAttackPatternSnapshot UEnemyAttackPatternData::MakeSnapshot() const
{
	return MakeSnapshot(ResolveHits());
}

FEnemyAttackPatternSnapshot UEnemyAttackPatternData::MakeSnapshot(TArray<FEnemyAttackHit> ResolvedHits) const
{
	FEnemyAttackPatternSnapshot Snapshot;
	Snapshot.PatternId = PatternId;
	Snapshot.DisplayName = DisplayName;
	Snapshot.TelegraphDuration = TelegraphDuration;
	Snapshot.AttackMontage = AttackMontage;
	Snapshot.Hits = MoveTemp(ResolvedHits);
	const FEnemyAttackHit& FirstHit = Snapshot.Hits[0];
	Snapshot.Damage = FirstHit.Damage;
	Snapshot.DefenseWindowDuration = FirstHit.DefenseWindowDuration;
	Snapshot.ParrySuccessDuration = FirstHit.ParrySuccessDuration;
	Snapshot.HitTime = FirstHit.HitTime;
	Snapshot.HitTimeOffset = FirstHit.HitTimeOffset;
	return Snapshot;
}

const UEnemyAttackPatternData* UEnemyAttackPatternData::SelectPattern(
	const TArray<TObjectPtr<UEnemyAttackPatternData>>& Patterns, FRandomStream& Random,
	FEnemyAttackPatternSnapshot* OutSnapshot,
	const TFunction<void(const UEnemyAttackPatternData*, const FString&)>& OnInvalid)
{
	struct FCandidate
	{
		const UEnemyAttackPatternData* Pattern;
		TArray<FEnemyAttackHit> Hits;
	};
	TArray<FCandidate> Candidates;
	double TotalWeight = 0.0;
	for (const UEnemyAttackPatternData* Pattern : Patterns)
	{
		TArray<FEnemyAttackHit> ResolvedHits;
		FString Reason;
		if (!IsValid(Pattern) || !Pattern->TryResolveValidHits(ResolvedHits, Reason))
		{
			if (OnInvalid) { OnInvalid(Pattern, IsValid(Pattern) ? Reason : TEXT("null or invalid reference")); }
			continue;
		}
		if (Pattern->SelectionWeight > 0.0f)
		{
			Candidates.Add({Pattern, MoveTemp(ResolvedHits)});
			TotalWeight += static_cast<double>(Pattern->SelectionWeight);
		}
	}
	if (Candidates.IsEmpty())
	{
		if (OutSnapshot) { *OutSnapshot = FEnemyAttackPatternSnapshot(); }
		return nullptr;
	}

	// 유효한 후보가 있을 때만 난수 하나를 소비하고 에셋 목록 순서대로 추첨한다.
	double Remaining = static_cast<double>(Random.GetFraction()) * TotalWeight;
	FCandidate* Selected = &Candidates.Last();
	for (FCandidate& Candidate : Candidates)
	{
		Remaining -= static_cast<double>(Candidate.Pattern->SelectionWeight);
		if (Remaining < 0.0)
		{
			Selected = &Candidate;
			break;
		}
	}
	if (OutSnapshot) { *OutSnapshot = Selected->Pattern->MakeSnapshot(MoveTemp(Selected->Hits)); }
	return Selected->Pattern;
}
