#pragma once
#include "CoreMinimal.h"
#include "Exploration/Encounters/BattleEncounterTrigger.h"
#include "BattleEncounterGroup.generated.h"
class UEncounterGroupData;
class UChildActorComponent;

// One persistent encounter ID; its composition can contain any number of species.
UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API ABattleEncounterGroup : public ABattleEncounterTrigger
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Encounter|Group") TObjectPtr<UEncounterGroupData> Composition;
	UFUNCTION(BlueprintCallable,CallInEditor,Category="Encounter|Group") void RebuildPreview();
	virtual TArray<AActor*> GetEncounterEnemies() const override;
	virtual void OnConstruction(const FTransform& Transform) override;
private:
	UPROPERTY() TArray<TObjectPtr<UChildActorComponent>> Members;
};
