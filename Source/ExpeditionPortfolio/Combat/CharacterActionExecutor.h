#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Combat/BattlePlayerCommand.h"
#include "CharacterActionExecutor.generated.h"
class UCharacterActionData;
UINTERFACE(BlueprintType,Blueprintable)
class EXPEDITIONPORTFOLIO_API UCharacterActionExecutor : public UInterface { GENERATED_BODY() };
class EXPEDITIONPORTFOLIO_API ICharacterActionExecutor
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Combat|Action") bool SupportsCharacterAction(UCharacterActionData* Action) const;
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Combat|Action") float GetCharacterActionDuration(UCharacterActionData* Action,bool bCounter) const;
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Combat|Action") bool StartCharacterAction(UCharacterActionData* Action,int64 ActionId,AActor* Target,bool bCounter);
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Combat|Action") void StopCharacterAction();
	// Compatibility with old Blueprint command slots; character profiles are the active source.
	UFUNCTION(BlueprintNativeEvent,Category="Combat|Legacy") UCharacterActionData* GetLegacyAction(EBattlePlayerCommand Command) const;
};
