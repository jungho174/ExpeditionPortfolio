#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BattlePlayerCommand.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "Combat/ParryCounterProvider.h"
#include "Combat/CharacterActionExecutor.h"
#include "UI/MagicProjectileFlight.h"
#include "MagicPresentationComponent.generated.h"

class UMagicActionData;
class UAnimMontage;
class UAnimInstance;
class USkeletalMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class APlayerCameraManager;
class UCameraShakeBase;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UMagicPresentationComponent : public UActorComponent, public IParryCounterProvider, public ICharacterActionExecutor
{
	GENERATED_BODY()
public:
 virtual bool SupportsCharacterAction_Implementation(UCharacterActionData* Action) const override;
 virtual float GetCharacterActionDuration_Implementation(UCharacterActionData* Action, bool bCounter) const override;
 virtual bool StartCharacterAction_Implementation(UCharacterActionData* Action, int64 ActionId, AActor* Target, bool bCounter) override;
 virtual void StopCharacterAction_Implementation() override { StopPresentation(); }
 virtual UCharacterActionData* GetLegacyAction_Implementation(EBattlePlayerCommand Command) const override;
	virtual float GetCounterActionDuration_Implementation() const override { return GetCounterDuration(); }
	virtual float GetCounterActionDamage_Implementation() const override;
	virtual bool StartCounterAction_Implementation(int64 ActionId, AActor* Target) override { return BeginCounterAction(ActionId, Target); }
	virtual void StopCounterAction_Implementation() override { StopPresentation(); }
	virtual void ReleaseCounterImpact_Implementation() override { RestoreCounterImpact(); }
	void ReleaseImpactForBattleResult() { RestoreCounterImpact(); }
	UMagicPresentationComponent();
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	TObjectPtr<UMagicActionData> BasicMagic;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	TObjectPtr<UMagicActionData> StrongMagic;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	TObjectPtr<UMagicActionData> CounterMagic;
	bool BeginCounterAction(int64 ActionId, AActor* Target);
	float GetCounterDuration() const;
	UFUNCTION(BlueprintPure, Category = "Combat|Defense")
	bool IsParryImpactActive() const { return bImpactHolding; }
	bool BeginMagicAction(EBattlePlayerCommand Command, int64 ActionId, AActor* Target);
	float GetExpectedDuration(EBattlePlayerCommand Command) const;
	void StopPresentation();
	void HandleHitNotify(USkeletalMeshComponent* SourceMesh, int32 MontageInstanceId);
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FMagicProjectileLifecycleTest;
	friend class FParryImpactRestoreTest;
#endif
	bool PlayStage(UAnimMontage* Montage, bool bCharge, float StartOverride = -1);
	void PauseCastForQTE(int64 ExpectedId);
	void FinishCastPreparation(int64 ExpectedId);
	FTimerHandle CastPauseTimer;
	float PreparationDuration = 0, CastHoldPosition = 0;
	bool bCastPreparing = false, bCastPaused = false;
	bool BeginAction(UMagicActionData* Data, int64 ActionId, AActor* Target, bool bCounter);
	bool ReportHit();
	void PlayAttackSound();
	void PlayImpactSound();
	TArray<AActor*> GetEffectTargets() const;
	FVector GetAimLocation() const;
	void SpawnAttackEffects();
	TArray<TWeakObjectPtr<AActor>> PresentationTargets;
	UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> AdditionalAttackFX;
	bool bAreaAction=false,bHitReported=false;
	bool bAttackSoundPlayed = false;
	bool UpdateCounterImpact(double RealTime);
	void RestoreCounterImpact();
	void FinishCharge(int64 ExpectedId);
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId);
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId);
	bool LaunchProjectile();
	void UpdateProjectile(float DeltaTime);
	void FinishAction(bool bInterrupted);
	void UpdateChargeAnchor();
	void ReleaseAttackEffect(TWeakObjectPtr<UNiagaraComponent> Effect);
	UNiagaraComponent* SpawnEffect(UNiagaraSystem* Effect, const FTransform& Transform, bool bAtTarget,AActor* EffectTarget=nullptr);
	UPROPERTY(Transient) TObjectPtr<UMagicActionData> ActiveData;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> ChargeFX;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> AttackFX;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> ProjectileFX;
	FMagicProjectileFlight ProjectileFlight;
	FTransform PendingImpactTransform;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> PendingImpactEffect;
	bool bAttackMontageFinished = false;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	TWeakObjectPtr<AActor> TargetActor;
	FTimerHandle ChargeTimer;
	TArray<TWeakObjectPtr<UNiagaraComponent>> FinishingEffects;
	int64 ActiveId = 0;
	int32 ActiveMontageInstanceId = INDEX_NONE;
	bool bCharging = false;
	bool bCounterAction = false;
	bool bImpactHolding = false;
	uint64 ImpactSlowId = 0;
	double ImpactEndsAt = 0;
	FTSTicker::FDelegateHandle ImpactTicker;
	TWeakObjectPtr<APlayerCameraManager> ImpactCamera;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> ParryImpactFX;
	UPROPERTY(Transient) TObjectPtr<UCameraShakeBase> ImpactShake;
};
