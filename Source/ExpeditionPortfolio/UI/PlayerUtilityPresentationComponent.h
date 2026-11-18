#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BattlePlayerCommand.h"
#include "Combat/CharacterActionExecutor.h"
#include "PlayerUtilityPresentationComponent.generated.h"

class UPlayerUtilityPresentationData;
class UMeleeActionData;
class UAnimMontage;
class USkeletalMeshComponent;
class UNiagaraComponent;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UPlayerUtilityPresentationComponent : public UActorComponent, public ICharacterActionExecutor
{
	GENERATED_BODY()
public:
 virtual bool SupportsCharacterAction_Implementation(UCharacterActionData* Action) const override;
 virtual float GetCharacterActionDuration_Implementation(UCharacterActionData* Action, bool bCounter) const override;
 virtual bool StartCharacterAction_Implementation(UCharacterActionData* Action, int64 ActionId, AActor* Target, bool bCounter) override;
 virtual void StopCharacterAction_Implementation() override { StopPresentation(); }
 virtual UCharacterActionData* GetLegacyAction_Implementation(EBattlePlayerCommand Command) const override { return nullptr; }
	UPlayerUtilityPresentationComponent();
	UPROPERTY(BlueprintReadOnly,Category="Legacy") TObjectPtr<UPlayerUtilityPresentationData> PresentationData;
	float GetExpectedDuration(EBattlePlayerCommand Command) const;
	bool BeginAction(EBattlePlayerCommand Command, int64 ActionId, AActor* Target);
	void StopPresentation();
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
 friend class FPlayerMeleeGroundingTest;
#endif
 UPROPERTY(Transient) TObjectPtr<UMeleeActionData> ActiveMeleeData;
 bool BeginMelee(UMeleeActionData* Data, int64 ActionId, AActor* Target, bool bCounter);
 void StartApproach();
 void FinishPreparation(int64 ExpectedId);
 bool bCounterAction=false;
 FTimerHandle PreparationTimer;
 void StartMelee();
	bool PlayTravelMontage();
	void UnbindTravelMontage();
	void OnTravelBlendingOut(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId);
	float ApproachHeight = 0, ReturnHeight = 0;
	void ApplyEffect(int64 ExpectedId);
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId);
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId);
	void Finish(bool bInterrupted);
	void RestoreMesh();
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> TravelMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ReturnMontage;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> PotionFX;
	FTimerHandle EffectTimer, FinishTimer;
	FTransform OriginalTransform;
	TArray<FVector> GroundTravelPoints;
	FVector TravelHome = FVector::ZeroVector, TravelMeshHome = FVector::ZeroVector;
	float Elapsed = 0, MoveDuration = 0, ReturnTime = 0, StrikeTime = 0;
	int64 ActiveId = 0;
	bool bPotionAction = false;
	TWeakObjectPtr<AActor> ActionTarget;
	bool bReturning = false, bMeshSaved = false;
};
