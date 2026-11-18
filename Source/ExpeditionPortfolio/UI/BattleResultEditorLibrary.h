#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BattleResultEditorLibrary.generated.h"

class UBlueprint;
UCLASS()
class EXPEDITIONPORTFOLIO_API UBattleResultEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,Category="Combat|Editor") static bool BuildResultLayout(UBlueprint* Blueprint,bool bMemberRow=false);
};
