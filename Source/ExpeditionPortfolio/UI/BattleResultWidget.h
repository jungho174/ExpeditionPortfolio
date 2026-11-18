#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/BattleResult.h"
#include "BattleResultWidget.generated.h"

class UBattleCoordinator;

UCLASS()
class EXPEDITIONPORTFOLIO_API UBattleResultBackdropWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	bool bVictory=false;
protected:
	virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
		FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const override;
};

UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API UBattleResultMemberWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Present(const FBattlePartyResult& Member);
};

UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API UBattleResultWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Result") TSubclassOf<UBattleResultMemberWidget> MemberWidgetClass;
	void Present(UBattleCoordinator* Battle);
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
	UFUNCTION() void ClickContinue();
	UFUNCTION() void ClickRetry();
	UFUNCTION() void ClickRetreat();
	void RefreshSelection();
	bool CanActivate() const;
	TWeakObjectPtr<UBattleCoordinator> Coordinator;
	bool bVictory=false;
	int32 Selection=0;
	double ShownAt=0;
};
