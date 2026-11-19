#pragma once

#include "CoreMinimal.h"
#include "Combat/BattleDefense.h"
#include "Combat/BattleState.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "DefensePresentationComponent.generated.h"

class UDefensePresentationData;
class UBattleCoordinator;
class USkeletalMeshComponent;
class UAnimMontage;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UDefensePresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDefensePresentationComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Defense Presentation")
	TObjectPtr<UDefensePresentationData> PresentationData;
	void StopPresentation();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FDefensePresentationRestoreTest;
	friend class FDefenseGroundingTest;
#endif
	void HandleDefenseAccepted(EBattleDefenseCommand Command, float TimeUntilHit);
	void HandleDefenseResult(EBattleDefenseResult Result);
	void HandleBattleState(EBattleState Previous, EBattleState Current);
	bool PlayMontage(UAnimMontage* Montage, float Rate, float StartTime = 0.0f);
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void StopGuard();
	void UpdateDodge(float DeltaTime);
	float PrepareDodgePath();
	TWeakObjectPtr<UBattleCoordinator> Battle;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
	FDelegateHandle AcceptedHandle;
	FDelegateHandle ResultHandle;
	FDelegateHandle StateHandle;
	FTimerHandle GuardTimer;
	FTransform OriginalMeshTransform;
	TArray<FVector> DodgeGroundPoints;
	FVector DodgeHome = FVector::ZeroVector, DodgeMeshHome = FVector::ZeroVector;
	float DodgeElapsed = 0.0f;
	float RetreatDuration = 0.0f;
	float ReturnStartTime = 0.0f;
	float ReturnDuration = 0.0f;
	float PoseTime = 0.0f;
	bool bDodging = false;
	bool bHoldingPose = false;
	bool bReturning = false;
};
