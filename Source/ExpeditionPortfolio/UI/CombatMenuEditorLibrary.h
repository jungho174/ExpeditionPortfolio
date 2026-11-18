#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatMenuEditorLibrary.generated.h"

class UBlueprint;
class UCombatHealthHUDData;

// Asset authoring only. No editor dependency in a packaged game.
UCLASS()
class EXPEDITIONPORTFOLIO_API UCombatMenuEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Combat|Editor")
	static bool BuildCommandLayout(UBlueprint* Blueprint);
	UFUNCTION(BlueprintCallable,Category="Combat|Editor")
	static bool ConfigurePartyHUD(UBlueprint* Blueprint,UCombatHealthHUDData* Appearance);
};
