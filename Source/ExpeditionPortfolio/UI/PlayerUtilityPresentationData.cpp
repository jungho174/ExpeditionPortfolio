#include "UI/PlayerUtilityPresentationData.h"
#include "Animation/AnimMontage.h"

bool UPlayerUtilityPresentationData::Validate(const bool bPotion) const
{
	const auto Positive = [](float Value) { return FMath::IsFinite(Value) && Value > 0 && Value < 30; };
	if (bPotion) { return Positive(PotionDuration) && Positive(PotionApplyTime) && PotionApplyTime < PotionDuration; }
	return MeleeMontage && Positive(MeleeMontage->GetPlayLength()) && FMath::IsNearlyEqual(MeleeMontage->RateScale, 1.0f)
		&& Positive(HitTime) && HitTime < MeleeMontage->GetPlayLength() && Positive(ApproachDuration)
		&& Positive(ReturnDuration) && FMath::IsFinite(TargetDistance) && TargetDistance > 0 && TargetDistance <= 500
		&& FMath::IsFinite(ApproachLeapHeight) && ApproachLeapHeight >= 0 && ApproachLeapHeight <= 100
		&& FMath::IsFinite(ReturnLeapHeight) && ReturnLeapHeight >= 0 && ReturnLeapHeight <= 100;
}
