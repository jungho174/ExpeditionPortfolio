#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatCompositionEditorLibrary.generated.h"
class UBlueprint;
class UActorComponent;
UCLASS()
class EXPEDITIONPORTFOLIO_API UCombatCompositionEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,Category="Combat|Editor") static UActorComponent* EnsureActionExecutor(UBlueprint* Blueprint,TSubclassOf<UActorComponent> ComponentClass,FName Name);
	// Keeps both asset paths and all placed legacy actor references; the legacy class becomes an alias child.
	UFUNCTION(BlueprintCallable,Category="Combat|Editor") static bool SplitEnemyContent(UBlueprint* Legacy,UBlueprint* Species);
};
