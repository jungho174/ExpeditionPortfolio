#include "UI/AnimNotify_MagicHit.h"
#include "UI/MagicPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_MagicHit::UAnimNotify_MagicHit()
{
	bIsNativeBranchingPoint = true;
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
#endif
}

void UAnimNotify_MagicHit::BranchingPointNotify(FBranchingPointNotifyPayload& Payload)
{
	if (Payload.SkelMeshComponent && Payload.SkelMeshComponent->GetOwner())
	{
		if (auto* Presentation = Payload.SkelMeshComponent->GetOwner()->FindComponentByClass<UMagicPresentationComponent>())
		{
			Presentation->HandleHitNotify(Payload.SkelMeshComponent, Payload.MontageInstanceID);
		}
	}
}
