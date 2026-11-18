#include "UI/BattleResultWidget.h"
#include "Combat/BattleCoordinator.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	void Text(UUserWidget* Widget,const TCHAR* Name,const FText& Value)
	{
		if (auto* Label=Cast<UTextBlock>(Widget->GetWidgetFromName(Name))) { Label->SetText(Value); }
	}
	FText HealthText(float Health,float Max)
	{
		return FText::Format(FText::FromString(TEXT("{0} / {1}")),FText::AsNumber(FMath::RoundToInt(Health)),FText::AsNumber(FMath::RoundToInt(Max)));
	}
}

void UBattleResultMemberWidget::Present(const FBattlePartyResult& Member)
{
	Text(this,TEXT("TXT_Name"),Member.Name);
	Text(this,TEXT("TXT_Health"),HealthText(Member.Health,Member.MaxHealth));
	if (auto* Bar=Cast<UProgressBar>(GetWidgetFromName(TEXT("PB_Health")))) { Bar->SetPercent(Member.MaxHealth>0?Member.Health/Member.MaxHealth:0); }
}

void UBattleResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (auto* B=Cast<UButton>(GetWidgetFromName(TEXT("BTN_Continue")))) { B->OnClicked.AddUniqueDynamic(this,&ThisClass::ClickContinue); }
	if (auto* B=Cast<UButton>(GetWidgetFromName(TEXT("BTN_Retry")))) { B->OnClicked.AddUniqueDynamic(this,&ThisClass::ClickRetry); }
	if (auto* B=Cast<UButton>(GetWidgetFromName(TEXT("BTN_Retreat")))) { B->OnClicked.AddUniqueDynamic(this,&ThisClass::ClickRetreat); }
}

void UBattleResultWidget::Present(UBattleCoordinator* Battle)
{
	Coordinator=Battle; ShownAt=FPlatformTime::Seconds(); Selection=0;
	const auto Result=Battle->GetBattleResult(); bVictory=Result.bVictory;
	if (auto* Backdrop=Cast<UBattleResultBackdropWidget>(GetWidgetFromName(TEXT("BG_Result")))) { Backdrop->bVictory=bVictory; }
	if (auto* Panel=GetWidgetFromName(TEXT("CN_Victory"))) { Panel->SetVisibility(bVictory?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed); }
	if (auto* Panel=GetWidgetFromName(TEXT("CN_Defeat"))) { Panel->SetVisibility(bVictory?ESlateVisibility::Collapsed:ESlateVisibility::SelfHitTestInvisible); }
	Text(this,TEXT("TXT_Dodges"),FText::AsNumber(Result.SuccessfulDodges));
	Text(this,TEXT("TXT_Parries"),FText::AsNumber(Result.SuccessfulParries));
	Text(this,TEXT("TXT_Dealt"),FText::AsNumber(FMath::RoundToInt(Result.DamageDealt)));
	Text(this,TEXT("TXT_Received"),FText::AsNumber(FMath::RoundToInt(Result.DamageReceived)));
	if (auto* List=Cast<UVerticalBox>(GetWidgetFromName(TEXT("VB_Party"))))
	{
		List->ClearChildren();
		if (bVictory && MemberWidgetClass)
		{
			for (const auto& Member:Result.Party)
			{
				auto* Row=CreateWidget<UBattleResultMemberWidget>(GetOwningPlayer(),MemberWidgetClass);
				if (!Row) { continue; }
				List->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,0,0,24)); Row->Present(Member);
			}
		}
	}
	RefreshSelection();
}

bool UBattleResultWidget::CanActivate() const
{
	return Coordinator.IsValid() && Coordinator->IsResultReady() && FPlatformTime::Seconds()-ShownAt>.25;
}
void UBattleResultWidget::ClickContinue() { if (CanActivate() && bVictory) { Coordinator->ContinueAfterVictory(); } }
void UBattleResultWidget::ClickRetry() { if (CanActivate() && !bVictory) { Coordinator->RetryBattle(); } }
void UBattleResultWidget::ClickRetreat()
{
	if (CanActivate() && !bVictory && !Coordinator->RetreatFromBattle())
	{
		Text(this,TEXT("TXT_Error"),FText::FromString(TEXT("복귀할 안전한 위치를 찾지 못했습니다.")));
	}
}
void UBattleResultWidget::RefreshSelection()
{
	if (auto* B=Cast<UButton>(GetWidgetFromName(TEXT("BTN_Retry")))) { B->SetBackgroundColor(Selection==0?FLinearColor(1,.65f,.55f):FLinearColor(.4f,.4f,.4f)); }
	if (auto* B=Cast<UButton>(GetWidgetFromName(TEXT("BTN_Retreat")))) { B->SetBackgroundColor(Selection==1?FLinearColor(1,.65f,.55f):FLinearColor(.4f,.4f,.4f)); }
}
FReply UBattleResultWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
	const FKey Key=Event.GetKey();
	if (Key==EKeys::SpaceBar || Key==EKeys::Enter)
	{
		if (!Event.IsRepeat()) { if (bVictory) { ClickContinue(); } else if (Selection==0) { ClickRetry(); } else { ClickRetreat(); } }
		return FReply::Handled();
	}
	if (!bVictory && (Key==EKeys::A || Key==EKeys::D || Key==EKeys::Left || Key==EKeys::Right))
	{
		if (!Event.IsRepeat()) { Selection=1-Selection; RefreshSelection(); } return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry,Event);
}

int32 UBattleResultBackdropWidget::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
	FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const
{
	const FVector2D Size=Geometry.GetLocalSize();
	const FLinearColor Accent=bVictory?FLinearColor(.72f,.6f,.35f,.4f):FLinearColor(.55f,.07f,.09f,.55f);
	// Continuous vertex gradients avoid seams from overlapping translucent strips.
	TArray<FSlateGradientStop> Horizontal;
	Horizontal.Emplace(FVector2D(0,0),FLinearColor(0,0,0,bVictory?.8f:.72f));
	Horizontal.Emplace(FVector2D(Size.X*.48f,0),FLinearColor(0,0,0,bVictory?.12f:.5f));
	Horizontal.Emplace(FVector2D(Size.X,0),FLinearColor(0,0,0,bVictory?.15f:.72f));
	FSlateDrawElement::MakeGradient(Elements,Layer,Geometry.ToPaintGeometry(),Horizontal,Orient_Vertical);
	if (bVictory)
	{
		TArray<FSlateGradientStop> Bottom;
		Bottom.Emplace(FVector2D(0,0),FLinearColor::Transparent);
		Bottom.Emplace(FVector2D(0,Size.Y*.68f),FLinearColor::Transparent);
		Bottom.Emplace(FVector2D(0,Size.Y),FLinearColor(0,0,0,.95f));
		FSlateDrawElement::MakeGradient(Elements,Layer+1,Geometry.ToPaintGeometry(),Bottom,Orient_Horizontal);
	}
	auto Line=[&](FVector2D A,FVector2D B)
	{
		TArray<FVector2D> Points{A*Size,B*Size};
		FSlateDrawElement::MakeLines(Elements,Layer+1,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,Accent,true,1.f);
	};
	if (bVictory) { Line({.04,.21},{.26,.21}); Line({.33,.77},{.67,.77}); }
	else
	{
		for (int32 I=0;I<5;++I)
		{
			const float D=.025f+I*.018f;
			Line({.5f-D,0},{.5f-D,.22f}); Line({.5f-D,.22f},{.5f,.34f});
			Line({.5f+D,0},{.5f+D,.22f}); Line({.5f+D,.22f},{.5f,.34f});
			Line({.5f,.6f},{.5f-D,.72f}); Line({.5f-D,.72f},{.5f-D,1});
			Line({.5f,.6f},{.5f+D,.72f}); Line({.5f+D,.72f},{.5f+D,1});
		}
	}
	return Super::NativePaint(Args,Geometry,Culling,Elements,Layer+2,Style,bEnabled);
}
