#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/AttackTiming.h"
#include "TimerManager.h"
#include "QTEPresentationComponent.generated.h"
class UActionQTESubsystem;
class UQTEPresentationData;
struct FQTESound;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UQTEPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UQTEPresentationComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|QTE") TObjectPtr<UQTEPresentationData> PresentationData;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void OnStarted(int64 Session);
	void OnResolved(int64 Session, EAttackTimingResult Result);
	void OnStepStarted(int64 Session, int32 Index);
	void OnStepResolved(int64 Session, int32 Index, EAttackTimingResult Result);
	void OnCancelled(int64 Session);
	void Queue(const FQTESound& Sound);
	void ClearTimers();
	TWeakObjectPtr<UActionQTESubsystem> QTE;
	FDelegateHandle StartHandle, ResultHandle, CancelHandle;
	FDelegateHandle StepStartHandle;
	TArray<FTimerHandle> Timers;
	int64 AudioSession = 0;
	bool bResultPlayed = false;
	TSet<int32> PlayedSteps;
};
