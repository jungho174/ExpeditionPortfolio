#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/EnemyAttackPatternData.h"
#include "Combat/BattleDefense.h"
#include "UI/EnemyDeathPresentation.h"
#include "EnemyAttackPresentationComponent.generated.h"

class UAnimMontage;
class USkeletalMeshComponent;
class UEnemyPresentationData;
class UCombatantComponent;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UEnemyAttackPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UEnemyAttackPresentationComponent();
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat|Presentation") TObjectPtr<UEnemyPresentationData> PresentationData;
	float GetExpectedDuration(UAnimMontage* Montage) const;
	bool HasHitReaction() const;
	void HoldForCounter();
	bool ReturnAfterCounter();
	bool IsHeldForCounter() const { return bHeldForCounter; }
	void PlayHitFeedback(const FEnemyAttackHit& Hit, EBattleDefenseResult Result);
	bool BeginAttack(UAnimMontage* Montage, int64 ActionId, AActor* Target);
	bool WasTravelBlocked() const { return bTravelBlocked; }
	void StopPresentation();
	void HandleHitNotify(USkeletalMeshComponent* SourceMesh, int32 MontageInstanceId, int32 HitIndex);
	UFUNCTION(BlueprintPure,Category="Combat|Presentation") bool IsDeathPresentationActive() const { return Death.IsActive(); }
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FEnemyDeathLifecycleTest;
	friend class FEnemyDeathFadeTest;
#endif
	void BeginDeathPresentation();
	void BeginDeathFade();
	void UpdateDeathFade(float DeltaTime);
	void FinishDeathPresentation();
	void ResetDeathPresentation();
	UPROPERTY(Transient) FEnemyDeathPresentation Death;
	FTimerHandle DeathTimer;
	bool StartStrike();
	void OnHealthChanged(float Before,float After);
	void OnMontageBlendingOut(UAnimMontage* Montage,bool bInterrupted,int64 ExpectedId);
	void StartTravel(bool bReturn);
	void Finish(bool bInterrupted);
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId);
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	int64 ActiveId = 0;
	int32 ActiveInstanceId = INDEX_NONE;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> StrikeMontage;
	TWeakObjectPtr<UCombatantComponent> Combatant;
	FDelegateHandle HealthHandle;
	FTransform OriginalTransform;
	FVector TravelOffset=FVector::ZeroVector;
	TArray<FVector> GroundTravelPoints;
	FVector TravelHome=FVector::ZeroVector,TravelMeshHome=FVector::ZeroVector;
	bool bTravelBlocked=false;
	float TravelElapsed=0, TravelDuration=0;
	bool bTravel=false,bReturning=false,bSavedTransform=false;
	bool bHeldForCounter=false;
};
