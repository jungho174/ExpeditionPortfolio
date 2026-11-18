#include "UI/BattleResultEditorLibrary.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "UI/BattleResultWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Brushes/SlateColorBrush.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
	template<class T> T* Make(UWidgetBlueprint* BP,const TCHAR* Name)
	{
		auto* W=BP->WidgetTree->ConstructWidget<T>(T::StaticClass(),Name);
		W->SetFlags(RF_Transactional); W->bIsVariable=true; BP->OnVariableAdded(W->GetFName()); return W;
	}
	void Place(UCanvasPanel* Parent,UWidget* W,FVector2D Anchor,FVector2D Offset,FVector2D Size,FVector2D Alignment=FVector2D::ZeroVector)
	{
		auto* S=Parent->AddChildToCanvas(W); S->SetAnchors(FAnchors(Anchor.X,Anchor.Y)); S->SetAlignment(Alignment); S->SetPosition(Offset); S->SetSize(Size);
	}
	void Fill(UCanvasPanel* Parent,UWidget* W)
	{
		auto* S=Parent->AddChildToCanvas(W); S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0));
	}
	UTextBlock* Label(UWidgetBlueprint* BP,const TCHAR* Name,const TCHAR* Value,int32 Size,FLinearColor Color=FLinearColor(.9f,.84f,.68f))
	{
		auto* T=Make<UTextBlock>(BP,Name); T->SetText(FText::FromString(Value)); auto Font=T->GetFont(); Font.Size=Size; T->SetFont(Font);
		T->SetColorAndOpacity(Color); T->SetShadowOffset(FVector2D(1,2)); T->SetShadowColorAndOpacity(FLinearColor(0,0,0,.85f));
		T->SetVisibility(ESlateVisibility::HitTestInvisible); return T;
	}
	void Button(UWidgetBlueprint* BP,UCanvasPanel* Panel,const TCHAR* Name,const TCHAR* LabelName,const TCHAR* Value,FVector2D Anchor,FVector2D Offset,FVector2D Size)
	{
		auto* B=Make<UButton>(BP,Name); B->IsFocusable=false;
		auto Style=B->GetStyle(); Style.SetNormal(FSlateColorBrush(FLinearColor(.04f,.035f,.03f,.9f)));
		Style.SetHovered(FSlateColorBrush(FLinearColor(.22f,.14f,.1f,.95f))); Style.SetPressed(FSlateColorBrush(FLinearColor(.35f,.23f,.15f,1)));
		B->SetStyle(Style); auto* T=Label(BP,LabelName,Value,22); T->SetJustification(ETextJustify::Center); B->SetContent(T);
		Place(Panel,B,Anchor,Offset,Size,FVector2D(.5f,.5f));
	}
}
#endif

bool UBattleResultEditorLibrary::BuildResultLayout(UBlueprint* Blueprint,bool bMemberRow)
{
#if WITH_EDITOR
	auto* BP=Cast<UWidgetBlueprint>(Blueprint);
	if (!BP || !BP->WidgetTree) { return false; }
	if (BP->WidgetTree->RootWidget)
	{
		if (!bMemberRow && !BP->WidgetTree->FindWidget(TEXT("BG_Result")))
		{
			auto* Canvas=Cast<UCanvasPanel>(BP->WidgetTree->RootWidget); if (!Canvas) { return false; }
			BP->Modify(); BP->WidgetTree->Modify();
			auto* Background=Make<UBattleResultBackdropWidget>(BP,TEXT("BG_Result"));
			Background->SetVisibility(ESlateVisibility::HitTestInvisible); Fill(Canvas,Background);
			CastChecked<UCanvasPanelSlot>(Background->Slot)->SetZOrder(-100);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		}
		return true;
	}
	BP->Modify(); BP->WidgetTree->Modify();
	auto* Root=Make<UCanvasPanel>(BP,TEXT("CN_ResultRoot")); BP->WidgetTree->RootWidget=Root;
	if (bMemberRow)
	{
		// SizeBox gives each dynamically populated party row a predictable desired height.
		auto* Box=Make<USizeBox>(BP,TEXT("SZ_Member")); Box->SetWidthOverride(340); Box->SetHeightOverride(130);
		BP->WidgetTree->RootWidget=Box; Box->SetContent(Root);
		auto* BG=Make<UBorder>(BP,TEXT("BDR_Member")); BG->SetBrushColor(FLinearColor(.018f,.02f,.025f,.87f)); Fill(Root,BG);
		Place(Root,Label(BP,TEXT("TXT_Name"),TEXT("플레이어"),28),{0,0},{24,14},{292,40});
		auto* Bar=Make<UProgressBar>(BP,TEXT("PB_Health")); Bar->SetFillColorAndOpacity(FLinearColor(.8f,.69f,.43f)); Bar->SetPercent(.6f);
		Place(Root,Bar,{0,0},{24,66},{292,8});
		Place(Root,Label(BP,TEXT("TXT_Health"),TEXT("60 / 100"),20),{0,0},{24,84},{292,30});
	}
	else
	{
		auto* Background=Make<UBattleResultBackdropWidget>(BP,TEXT("BG_Result")); Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		Fill(Root,Background); CastChecked<UCanvasPanelSlot>(Background->Slot)->SetZOrder(-100);
		auto* Win=Make<UCanvasPanel>(BP,TEXT("CN_Victory")); Fill(Root,Win);
		Place(Win,Label(BP,TEXT("TXT_Victory"),TEXT("승리"),76),{0,0},{70,44},{380,130});
		auto* List=Make<UVerticalBox>(BP,TEXT("VB_Party")); Place(Win,List,{1,.23f},{-70,0},{340,580},{1,0});
		auto* Stats=Make<UCanvasPanel>(BP,TEXT("CN_Stats")); Place(Win,Stats,{.5f,1},{0,-130},{620,145},{.5f,1});
		const TCHAR* Names[]={TEXT("TXT_Dealt"),TEXT("TXT_Received"),TEXT("TXT_Parries"),TEXT("TXT_Dodges")};
		const TCHAR* Labels[]={TEXT("준 피해"),TEXT("받은 피해"),TEXT("패링 성공"),TEXT("회피 성공")};
		for (int32 I=0;I<4;++I)
		{
			const FVector2D P((I/2)*320,(I%2)*58);
			const FString Caption=FString::Printf(TEXT("TXT_StatLabel%d"),I);
			Place(Stats,Label(BP,*Caption,Labels[I],22),{0,0},P,{170,40});
			Place(Stats,Label(BP,Names[I],TEXT("0"),26),{0,0},P+FVector2D(180,0),{130,42});
		}
		Button(BP,Win,TEXT("BTN_Continue"),TEXT("TXT_Continue"),TEXT("Space   계속"),{.5f,1},{0,-64},{290,55});
		auto* Lose=Make<UCanvasPanel>(BP,TEXT("CN_Defeat")); Fill(Root,Lose);
		auto* Title=Label(BP,TEXT("TXT_Defeat"),TEXT("패배"),90,FLinearColor(.7f,.07f,.09f)); Title->SetJustification(ETextJustify::Center);
		Place(Lose,Title,{.5f,.46f},{0,0},{500,145},{.5f,.5f});
		Button(BP,Lose,TEXT("BTN_Retry"),TEXT("TXT_Retry"),TEXT("재도전"),{.5f,.79f},{-145,0},{250,64});
		Button(BP,Lose,TEXT("BTN_Retreat"),TEXT("TXT_Retreat"),TEXT("후퇴"),{.5f,.79f},{145,0},{250,64});
		auto* Hint=Label(BP,TEXT("TXT_DefeatHint"),TEXT("A / D   선택       Space   확정"),17,FLinearColor(.65f,.61f,.57f)); Hint->SetJustification(ETextJustify::Center);
		Place(Lose,Hint,{.5f,.87f},{0,0},{650,40},{.5f,.5f});
		auto* Error=Label(BP,TEXT("TXT_Error"),TEXT(""),17,FLinearColor(.85f,.35f,.3f)); Error->SetJustification(ETextJustify::Center);
		Place(Lose,Error,{.5f,.93f},{0,0},{800,40},{.5f,.5f});
		Lose->SetVisibility(ESlateVisibility::Collapsed);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); return true;
#else
	return false;
#endif
}
