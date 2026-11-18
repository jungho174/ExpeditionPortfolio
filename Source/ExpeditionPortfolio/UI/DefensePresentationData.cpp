#include "UI/DefensePresentationData.h"
#include "Animation/AnimMontage.h"

bool UDefensePresentationData::Validate(FString& Reason) const
{
	Reason.Reset();
	const float Positive[] = {ParryPlayRate, ParryDuration, DodgeRetreatDuration, DodgeReturnDuration, DodgePoseTime, HitReactionPlayRate};
	for (const float Value : Positive)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0f) { Reason = TEXT("Presentation durations and rates must be finite and positive"); return false; }
	}
	const float NonNegative[] = {ParryStartTime, DodgeDistance, DodgeHoldAfterHit};
	for (const float Value : NonNegative)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f) { Reason = TEXT("Presentation offsets must be finite and non-negative"); return false; }
	}
	if ((ParryMontage && (ParryMontage->IsValidAdditive() || ParryStartTime >= ParryMontage->GetPlayLength()))
		|| (DodgeMontage && (DodgeMontage->IsValidAdditive() || DodgePoseTime >= DodgeMontage->GetPlayLength()))
		|| (HitReactionMontage && !HitReactionMontage->IsValidAdditive()))
	{
		Reason = TEXT("Guard/dodge require full poses, hit reaction requires an additive montage, and sample times must fit");
		return false;
	}
	return true;
}
