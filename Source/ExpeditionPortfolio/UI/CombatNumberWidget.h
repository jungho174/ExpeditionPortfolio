#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/CombatNumber.h"
#include "Combat/BattleState.h"
#include "CombatNumberWidget.generated.h"

class UCombatNumberData;
class UBattleCoordinator;

UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API UCombatNumberWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Numbers") TObjectPtr<UCombatNumberData> PresentationData;
	UFUNCTION(BlueprintPure, Category="Combat|Numbers") int32 GetActiveNumberCount() const { return Entries.Num(); }
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
	struct FEntry
	{
		TWeakObjectPtr<AActor> Target;
		FText Label;
		ECombatNumberKind Kind=ECombatNumberKind::Damage;
		double StartedAt=0;
		FVector LastWorldPosition=FVector::ZeroVector;
		float LateralOffset=0;
	};
	void ShowNumber(const FCombatNumber& Number);
	void HandleState(EBattleState Before, EBattleState After);
	void ClearNumbers();
	TArray<FEntry> Entries;
	TWeakObjectPtr<UBattleCoordinator> Battle;
	FDelegateHandle NumberHandle, StateHandle;
};
