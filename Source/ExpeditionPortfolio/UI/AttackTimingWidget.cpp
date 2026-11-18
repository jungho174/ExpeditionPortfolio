#include "UI/AttackTimingWidget.h"
#include "Combat/ActionQTESubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Brushes/SlateRoundedBoxBrush.h"

void UAttackTimingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	QTE = GetWorld()->GetSubsystem<UActionQTESubsystem>();
	if (QTE.IsValid())
	{
		TimingHandle = QTE->Changed.AddUObject(this, &ThisClass::Refresh);
	}
	Refresh();
}
void UAttackTimingWidget::NativeDestruct()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(FeedbackTimer); }
	if (QTE.IsValid())
	{
		QTE->Changed.Remove(TimingHandle);
	}
	Super::NativeDestruct();
}
void UAttackTimingWidget::HideFeedback() { SetVisibility(ESlateVisibility::Collapsed); }
void UAttackTimingWidget::Refresh()
{
	const bool bWasActive = View.bActive;
	View = QTE.IsValid() ? QTE->GetView() : FAttackTimingView();
	if (!QTE.IsValid()
		|| View.Result == EAttackTimingResult::None || View.Result == EAttackTimingResult::Cancelled)
	{
		GetWorld()->GetTimerManager().ClearTimer(FeedbackTimer);
		HideFeedback();
	}
	else if (View.bActive)
	{
		GetWorld()->GetTimerManager().ClearTimer(FeedbackTimer);
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else if (bWasActive)
	{
		GetWorld()->GetTimerManager().SetTimer(FeedbackTimer, this, &ThisClass::HideFeedback, 0.45f, false);
	}
}
void UAttackTimingWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (QTE.IsValid() && View.bActive) { View = QTE->GetView(); }
}
int32 UAttackTimingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, Culling, Elements, Layer, Style, bEnabled);
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Center(Size.X * 0.5, Size.Y * 0.42);
	const float R = FMath::Clamp(Radius, 35.0f, 160.0f);
	const auto Text = [&](const FText& Label, FVector2D Position, FLinearColor Color)
	{
		FSlateFontInfo Font=InputFont;
		if (!Font.HasValidFont()) { Font=GetDefault<UTextBlock>()->GetFont(); Font.Size=16; }
		const FVector2D Extent=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label,Font);
		FSlateDrawElement::MakeText(Elements, ++Layer, Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(Position-Extent*.5)), Label, Font, ESlateDrawEffect::None, Color);
	};
	const auto DrawGauge = [&](FVector2D GaugeCenter, float GaugeRadius, float Progress, float SuccessStart, bool bShowRemaining)
	{
	const FVector2D Corners[] = { GaugeCenter+FVector2D(0,-GaugeRadius), GaugeCenter+FVector2D(GaugeRadius,0), GaugeCenter+FVector2D(0,GaugeRadius), GaugeCenter+FVector2D(-GaugeRadius,0), GaugeCenter+FVector2D(0,-GaugeRadius) };
	const auto Point = [&](float Progress)
	{
		const float P = FMath::Clamp(Progress, 0.0f, 1.0f) * 4;
		const int32 Edge = FMath::Min(FMath::FloorToInt(P), 3);
		return FMath::Lerp(Corners[Edge], Corners[Edge+1], P-Edge);
	};
	const auto Stroke = [&](float Start, float End, FLinearColor Color, float Width)
	{
		if (End <= Start) { return; }
		TArray<FVector2D> Points; Points.Add(Point(Start));
		for (int32 I=1; I<4; ++I) { if (I*.25f > Start && I*.25f < End) { Points.Add(Corners[I]); } }
		Points.Add(Point(End));
		FSlateDrawElement::MakeLines(Elements, ++Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Width);
	};
	const float Width = FMath::Clamp(LineWidth, 1.0f, 10.0f);
	Stroke(0,1,FLinearColor(1,1,1,0.15f),Width);
	if (bShowRemaining)
	{
		Stroke(Progress,SuccessStart,FillColor,Width);
		Stroke(FMath::Max(Progress,SuccessStart),1,WindowColor,Width);
	}
	static const FSlateRoundedBoxBrush Disc(FLinearColor(0.015f,0.018f,0.025f,0.94f), 36.0f);
	FSlateDrawElement::MakeBox(Elements, ++Layer, Geometry.ToPaintGeometry(FVector2D(72,72), FSlateLayoutTransform(GaugeCenter-FVector2D(36,36))), &Disc, ESlateDrawEffect::None, FLinearColor(0.015f,0.018f,0.025f,0.94f));
	TArray<FVector2D> Ring;
	for (int32 I=0; I<=64; ++I) { const float A=I*2*PI/64; Ring.Add(GaugeCenter+FVector2D(FMath::Cos(A),FMath::Sin(A))*36); }
	FSlateDrawElement::MakeLines(Elements, ++Layer, Geometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, FLinearColor(0.9f,0.86f,0.7f), true, 2);
	Text(InputLabel,GaugeCenter,FLinearColor::White);
	};
	if (View.Mode==EQTEPatternMode::Rhythm)
	{
		const int32 Columns=FMath::Min(View.StepCount,4), Rows=FMath::DivideAndRoundUp(View.StepCount,Columns);
		const float GaugeR=R*(View.StepCount<=2?.78f:.48f);
		for (const FQTEGaugeView& Gauge:View.Gauges)
		{
			const FVector2D Offset((Gauge.StepIndex%Columns-(Columns-1)*.5f)*GaugeR*2.25f,
				(Gauge.StepIndex/Columns-(Rows-1)*.5f)*GaugeR*2.65f);
			DrawGauge(Center+Offset,GaugeR,Gauge.Progress,Gauge.SuccessStart,true);
		}
	}
	else { DrawGauge(Center,R,View.Progress,View.SuccessStart,!View.bWaitingBetweenSteps); }
	return Layer;
}
