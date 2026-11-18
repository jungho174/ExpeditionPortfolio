#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_EnemyHit.generated.h"

UCLASS(meta = (DisplayName = "Enemy Hit"))
class EXPEDITIONPORTFOLIO_API UAnimNotify_EnemyHit : public UAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0")) int32 HitIndex = 0;
	UAnimNotify_EnemyHit();
	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& Payload) override;
};
