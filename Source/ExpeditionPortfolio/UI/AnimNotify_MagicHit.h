#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_MagicHit.generated.h"

UCLASS()
class EXPEDITIONPORTFOLIO_API UAnimNotify_MagicHit : public UAnimNotify
{
	GENERATED_BODY()
public:
	UAnimNotify_MagicHit();
	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& Payload) override;
};
