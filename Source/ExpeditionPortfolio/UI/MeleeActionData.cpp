#include "UI/MeleeActionData.h"
#include "Animation/AnimMontage.h"
bool UMeleeActionData::IsValidPresentation() const
{
	return AttackMontage && FMath::IsFinite(HitTime) && HitTime>0 && HitTime<AttackMontage->GetPlayLength()
		&& FMath::IsFinite(ApproachDuration) && ApproachDuration>0 && FMath::IsFinite(ReturnDuration) && ReturnDuration>0
		&& FMath::IsFinite(TargetDistance) && TargetDistance>=1 && FMath::IsFinite(ApproachLeapHeight) && ApproachLeapHeight>=0
		&& FMath::IsFinite(ReturnLeapHeight) && ReturnLeapHeight>=0;
}
