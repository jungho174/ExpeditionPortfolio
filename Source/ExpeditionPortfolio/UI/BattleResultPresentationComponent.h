#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Ticker.h"
#include "Combat/BattleState.h"
#include "BattleResultPresentationComponent.generated.h"

class UBattleResultPresentationData;
class UBattleResultWidget;
class UBattleCoordinator;
class APlayerController;

UCLASS(ClassGroup=(Combat),meta=(BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UBattleResultPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBattleResultPresentationComponent();
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat|Result") TObjectPtr<UBattleResultPresentationData> PresentationData;
	void RestoreWorld();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void HandleState(EBattleState Before,EBattleState After);
	void ShowResult();
	UPROPERTY(Transient) TObjectPtr<UBattleResultWidget> Widget;
	TWeakObjectPtr<UBattleCoordinator> Coordinator;
	TWeakObjectPtr<APlayerController> Controller;
	FDelegateHandle StateHandle;
	FTSTicker::FDelegateHandle SlowTicker;
	double SlowEndsAt=0;
	uint64 ResultSlowId = 0;
	bool bActive=false;
	bool bPausedByResult=false;
};
