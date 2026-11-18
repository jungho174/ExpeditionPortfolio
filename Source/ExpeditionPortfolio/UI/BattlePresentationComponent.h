#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BattleState.h"
#include "Combat/AttackTiming.h"
#include "UI/BattleUIAudioData.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "BattlePresentationComponent.generated.h"

class UBattleCoordinator;
class UBattlePresentationData;
class ABattleCameraRig;
class APlayerController;
struct FBattleCameraShot;
class UParryFeedbackData;
class AWorldSettings;
class UNiagaraComponent;
class UCameraShakeBase;
class UCharacterActionData;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UBattlePresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat|Parry") TObjectPtr<UParryFeedbackData> ParryFeedback;
	bool BeginParryFeedback(int64 ActionId, bool bComplete);
	void ReleaseParryFeedback();
	void PresentCounterAction(UCharacterActionData* Action);
	UBattlePresentationComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Camera") TObjectPtr<UBattlePresentationData> PresentationData;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio") TObjectPtr<UBattleUIAudioData> UIAudioData;
	void PlayUIAudio(EBattleUIAudio Cue);
	float BeginBattleIntro(UBattleCoordinator* Battle, int64 BattleId);
	float GetTargetConfirmationDuration() const;
	UFUNCTION(BlueprintPure, Category="Combat|Camera") FName GetCurrentShot() const { return CurrentShot; }
	UFUNCTION(BlueprintCallable, Category="Combat|Camera") void RestoreExplorationCamera();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	FDelegateHandle DefenseHandle;
	FTSTicker::FDelegateHandle ParryTicker;
	TWeakObjectPtr<AWorldSettings> SlowWorld;
	float PreviousParryDilation=1;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> ParryFX;
	UPROPERTY(Transient) TObjectPtr<UCameraShakeBase> ParryShake;
	void HandleState(EBattleState Before, EBattleState After);
	void HandlePhase(EPlayerActionPhase Phase);
	void HandleTargetSelection();
	TWeakObjectPtr<AActor> CameraTarget;
	TWeakObjectPtr<AActor> CameraSubject;
	TWeakObjectPtr<AActor> LastSelectionSoundTarget;
	FDelegateHandle TargetHandle;
	FDelegateHandle TargetMoveHandle;
	FDelegateHandle TargetConfirmHandle;
	void ShowShot(const FBattleCameraShot& Shot, FName Name);
	TWeakObjectPtr<UBattleCoordinator> Coordinator;
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<AActor> ExplorationViewTarget;
	FRotator ExplorationRotation;
	UPROPERTY(Transient) TArray<TObjectPtr<ABattleCameraRig>> Rigs;
	FDelegateHandle StateHandle;
	FDelegateHandle PhaseHandle;
	FTimerHandle IntroTimer;
	int32 RigIndex = 0;
	bool bPresenting = false;
	FName CurrentShot;
};
