#include "UI/AnimNotify_EnemyHit.h"
#include "UI/EnemyAttackPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_EnemyHit::UAnimNotify_EnemyHit()
{
	bIsNativeBranchingPoint = true;
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
#endif
}

void UAnimNotify_EnemyHit::BranchingPointNotify(FBranchingPointNotifyPayload& Payload)
{
	if (Payload.SkelMeshComponent && Payload.SkelMeshComponent->GetOwner())
	{
		if (auto* Presentation = Payload.SkelMeshComponent->GetOwner()->FindComponentByClass<UEnemyAttackPresentationComponent>())
		{
			Presentation->HandleHitNotify(Payload.SkelMeshComponent, Payload.MontageInstanceID, HitIndex);
		}
	}
}
