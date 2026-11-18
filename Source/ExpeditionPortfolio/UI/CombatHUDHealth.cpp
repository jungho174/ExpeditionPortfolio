#include "UI/CombatHUDWidget.h"
#include "UI/CombatHealthHUDData.h"
#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Combat/CombatantVisualData.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"

int32 UCombatHUDWidget::PaintHealthHUD(const FGeometry& Geometry,FSlateWindowElementList& Elements,int32 Layer) const
{
	if (!BattleCoordinator.IsValid() || !GetOwningPlayer()) { return Layer; }
	const auto State=BattleCoordinator->GetCurrentState();
	if (State==EBattleState::Exploration || State==EBattleState::Victory || State==EBattleState::Defeat || State==EBattleState::ReturnToField) { return Layer; }
	const auto* Data=HealthHUDData ? HealthHUDData.Get() : GetDefault<UCombatHealthHUDData>();
	const FVector2D Size=Geometry.GetLocalSize();
	const auto Box=[&](FVector2D Position,FVector2D Extent,FLinearColor Color)
	{
		if (Extent.X>0 && Extent.Y>0) { FSlateDrawElement::MakeBox(Elements,++Layer,Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(Position)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color); }
	};
	const auto Lines=[&](const TArray<FVector2D>& Points,FLinearColor Color,float Thickness=1.f)
	{ FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Thickness); };
	const auto Text=[&](const FText& Value,FVector2D Position,FSlateFontInfo Font,FLinearColor Color,int32 DefaultSize,float RightEdge=0.f)
	{
		if (!Font.HasValidFont()) { Font=GetDefault<UTextBlock>()->GetFont(); Font.Size=DefaultSize; }
		if (RightEdge>0.f)
		{
			const FVector2D Extent=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,Font);
			Position.X=FMath::Min(Position.X,RightEdge-Extent.X);
		}
		FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(Position+FVector2D(1,1))),Value,Font,ESlateDrawEffect::None,FLinearColor::Black);
		FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(Position)),Value,Font,ESlateDrawEffect::None,Color);
	};
	const auto Allies=BattleCoordinator->GetBattleAllies();
	const float Available=FMath::Max(120.f,static_cast<float>(Size.X)-Data->BottomRightMargin.X-24.f);
	const float Scale=FMath::Min(1.f,Available/FMath::Max(1.f,Allies.Num()*(Data->PanelWidth+Data->PanelGap)));
	const float Width=Data->PanelWidth*Scale, Gap=Data->PanelGap*Scale;
	const float StartX=Size.X-Data->BottomRightMargin.X-Allies.Num()*Width-FMath::Max(0,Allies.Num()-1)*Gap;
	for (int32 Index=0;Index<Allies.Num();++Index)
	{
		AActor* Ally=Allies[Index];
		const auto* Health=Ally->FindComponentByClass<UCombatantComponent>();
		if (!Health) { continue; }
		const FVector2D P(StartX+Index*(Width+Gap),Size.Y-Data->BottomRightMargin.Y-Data->PanelHeight);
		const bool bActive=BattleCoordinator->GetActingCombatant()==Ally;
		const FLinearColor Accent=bActive?Data->ActiveColor:FLinearColor(.5f,.52f,.55f);
		Box(P+FVector2D(0,63),FVector2D(Width,Data->PanelHeight-63),FLinearColor(.015f,.02f,.025f,.9f));
		if (Data->bShowPartyPortrait)
		{
			const FVector2D Center=P+FVector2D(Width*.5,46);
			Lines({Center+FVector2D(0,-45),Center+FVector2D(45,0),Center+FVector2D(0,45),Center+FVector2D(-45,0),Center+FVector2D(0,-45)},Accent,bActive?2.f:1.f);
			const auto* Visual=UCombatantVisualData::Find(Ally);
			if (Visual && Visual->Portrait)
			{
				FSlateBrush Portrait; Portrait.SetResourceObject(Visual->Portrait); Portrait.ImageSize=FVector2D(62,62); Portrait.DrawAs=ESlateBrushDrawType::Image;
				FSlateDrawElement::MakeBox(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(62,62),FSlateLayoutTransform(Center-FVector2D(31,31))),&Portrait,ESlateDrawEffect::None,FLinearColor::White);
			}
			else
			{
				// Neutral mannequin silhouette until this character supplies a portrait.
				TArray<FVector2D> Head;
				for (int32 I=0;I<=20;++I) { const float A=I*2.f*PI/20; Head.Add(Center+FVector2D(FMath::Cos(A)*10,FMath::Sin(A)*12-10)); }
				Lines(Head,FLinearColor(.72f,.74f,.75f),3);
				Lines({Center+FVector2D(-22,25),Center+FVector2D(-16,10),Center+FVector2D(0,4),Center+FVector2D(16,10),Center+FVector2D(22,25)},FLinearColor(.72f,.74f,.75f),6);
			}
		}
		FText Name=Health->CombatDisplayName;
		if (Name.IsEmpty() || Name.ToString()==TEXT("적")) { Name=FText::FromString(Index==0?TEXT("플레이어"):TEXT("동료")); }
		Text(Name,P+FVector2D(12,74),Data->NameFont,Accent,15);
		const float Ratio=Health->GetMaxHealth()>0?FMath::Clamp(Health->GetCurrentHealth()/Health->GetMaxHealth(),0.f,1.f):0;
		Box(P+FVector2D(9,104),FVector2D(Width-18,15),FLinearColor(.12f,.13f,.15f));
		Box(P+FVector2D(10,105),FVector2D((Width-20)*Ratio,13),Data->PartyHealthColor);
		Text(FText::Format(NSLOCTEXT("CombatHUD","PartyHP","{0} / {1}"),FText::AsNumber(FMath::CeilToInt(Health->GetCurrentHealth())),FText::AsNumber(FMath::CeilToInt(Health->GetMaxHealth()))),P+FVector2D(Width*.48,96),Data->HealthFont,FLinearColor::White,20,P.X+Width-10.f);
		Lines({P+FVector2D(9,Data->PanelHeight-12),P+FVector2D(Width-9,Data->PanelHeight-12)},Accent);
	}
	int32 PixelWidth=0,PixelHeight=0;
	GetOwningPlayer()->GetViewportSize(PixelWidth,PixelHeight);
	if (PixelWidth<=0 || PixelHeight<=0) { return Layer; }
	for (AActor* Enemy:BattleCoordinator->GetBattleEnemies())
	{
		const auto* Health=Enemy->FindComponentByClass<UCombatantComponent>();
		if (!Health || Health->IsDefeated()) { continue; }
		FVector2D Screen;
		if (!GetOwningPlayer()->ProjectWorldLocationToScreen(UCombatantVisualData::ResolveAnchor(Enemy,ECombatantAnchor::HealthBar),Screen,true)) { continue; }
		const FVector2D Center=Screen*Size/FVector2D(PixelWidth,PixelHeight);
		const FVector2D Extent=Data->EnemyBarSize;
		const FVector2D P=Center-Extent*.5;
		Box(P-FVector2D(1,1),Extent+FVector2D(2,2),FLinearColor(.65f,.65f,.65f));
		Box(P,Extent,FLinearColor(.09f,.09f,.1f));
		Box(P,Extent*FVector2D(FMath::Clamp(Health->GetCurrentHealth()/FMath::Max(1.f,Health->GetMaxHealth()),0.f,1.f),1),Data->EnemyHealthColor);
	}
	return Layer;
}
