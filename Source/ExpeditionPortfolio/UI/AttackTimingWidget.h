#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Combat/AttackTiming.h"
#include "Combat/BattleState.h"
#include "TimerManager.h"
#include "AttackTimingWidget.generated.h"

class UActionQTESubsystem;

UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API UAttackTimingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing|Appearance") FText InputLabel = FText::FromString(TEXT("SPACE"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing|Appearance") FSlateFontInfo InputFont;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing|Appearance") float Radius = 86;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing|Appearance") float LineWidth = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing|Appearance") FLinearColor FillColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing|Appearance") FLinearColor WindowColor = FLinearColor(1,0.8f,0.2f);
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
	void Refresh();
	void HideFeedback();

	TWeakObjectPtr<UActionQTESubsystem> QTE;
	FAttackTimingView View;
	FDelegateHandle TimingHandle;

	FTimerHandle FeedbackTimer;
};
