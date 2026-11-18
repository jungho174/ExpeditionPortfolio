#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemPresentationComponent.generated.h"
class UCombatItemData;
class UCharacterItemUseData;
class UAnimMontage;
class UNiagaraComponent;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(Combat),meta=(BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UItemPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UItemPresentationComponent();
	float GetExpectedDuration() const;
	bool BeginUse(UCombatItemData* Item,int64 ActionId,AActor* Target);
	void StopPresentation();
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UCharacterItemUseData* GetCharacterSettings() const;
	void Apply(int64 ExpectedId);
	void Finish(bool bInterrupted);
	void OnMontageEnded(UAnimMontage* Montage,bool bInterrupted,int64 ExpectedId);
	UPROPERTY(Transient) TObjectPtr<UCombatItemData> ActiveItem;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Effect;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	TWeakObjectPtr<AActor> Target;
	FTimerHandle ApplyTimer,FinishTimer;
	int64 ActiveId=0;
};
