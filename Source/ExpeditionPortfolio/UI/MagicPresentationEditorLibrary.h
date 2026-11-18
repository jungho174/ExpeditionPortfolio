#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicPresentationEditorLibrary.generated.h"

class UNiagaraSystem;

UCLASS()
class EXPEDITIONPORTFOLIO_API UMagicPresentationEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// Make the complete fireball follow component motion and disable source collision; combat owns arrival.
	UFUNCTION(BlueprintCallable, Category="Combat|Editor")
	static bool PrepareProjectileSystem(UNiagaraSystem* System, UNiagaraSystem* ReferenceSystem);
};
