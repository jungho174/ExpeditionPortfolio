#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ParryCounterProvider.generated.h"

// A character supplies its own counter animation/weapon/projectile implementation.
// The battle owns eligibility, damage and duplicate protection.
UINTERFACE(BlueprintType, Blueprintable)
class EXPEDITIONPORTFOLIO_API UParryCounterProvider : public UInterface
{
	GENERATED_BODY()
};

class EXPEDITIONPORTFOLIO_API IParryCounterProvider
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combat|Counter") float GetCounterActionDuration() const;
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combat|Counter") float GetCounterActionDamage() const;
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combat|Counter") bool StartCounterAction(int64 ActionId, AActor* Target);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combat|Counter") void StopCounterAction();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combat|Counter") void ReleaseCounterImpact();
};
