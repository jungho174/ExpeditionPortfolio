#include "UI/CombatNumberWidget.h"
#include "Combat/CombatantVisualData.h"
#include "UI/CombatNumberData.h"
#include "Combat/BattleCoordinator.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	float Bounded(float Value, float Min, float Max, float Fallback)
	{
		return FMath::IsFinite(Value)?FMath::Clamp(Value,Min,Max):Fallback;
	}
}

void UCombatNumberWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::Collapsed);
	Battle=GetWorld()->GetSubsystem<UBattleCoordinator>();
	if (Battle.IsValid())
	{
		NumberHandle=Battle->OnCombatNumber().AddUObject(this,&ThisClass::ShowNumber);
		StateHandle=Battle->OnBattleStateChanged().AddUObject(this,&ThisClass::HandleState);
	}
}

void UCombatNumberWidget::NativeDestruct()
{
	if (Battle.IsValid())
	{
		Battle->OnCombatNumber().Remove(NumberHandle);
		Battle->OnBattleStateChanged().Remove(StateHandle);
	}
	Entries.Reset();
	Super::NativeDestruct();
}

void UCombatNumberWidget::ClearNumbers()
{
	Entries.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UCombatNumberWidget::HandleState(EBattleState Before, EBattleState After)
{
	if (After==EBattleState::Exploration || After==EBattleState::BattleIntro || After==EBattleState::ReturnToField) { ClearNumbers(); }
}

void UCombatNumberWidget::ShowNumber(const FCombatNumber& Number)
{
	if (!PresentationData || !IsValid(Number.Target) || !FMath::IsFinite(Number.Amount) || Number.Amount<0) { return; }
	FEntry Entry;
	Entry.Target=Number.Target;
	Entry.Kind=Number.Kind;
	Entry.StartedAt=GetWorld()->GetTimeSeconds();
	Entry.LastWorldPosition=UCombatantVisualData::ResolveAnchor(Number.Target,ECombatantAnchor::DamageNumber);
	// Fixed comma grouping for combat numerals, independent of the editor's locale.
	FNumberFormattingOptions Format;
	Format.SetUseGrouping(true).SetMaximumFractionalDigits(0).SetMinimumFractionalDigits(0);
	Entry.Label=FText::AsNumber(FMath::RoundToDouble(Number.Amount),&Format,FInternationalization::Get().GetCulture(TEXT("en-US")));
	if (Number.Kind==ECombatNumberKind::Healing)
	{
		Entry.Label=FText::Format(FText::FromString(TEXT("+{0}")),Entry.Label);
	}
	int32 Nearby=0;
	for (const FEntry& Existing:Entries) { if (Existing.Target==Entry.Target && Entry.StartedAt-Existing.StartedAt<.3) { ++Nearby; } }
	Entry.LateralOffset=Nearby==0?0:((Nearby%2)?1:-1)*28.f*((Nearby+1)/2);
	if (Entries.Num()>=32) { Entries.RemoveAt(0); }
	Entries.Add(MoveTemp(Entry));
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UE_LOG(LogBattleCoordinator,Log,TEXT("COMBAT_NUMBER Target=%s Amount=%.2f Kind=%d"),*GetNameSafe(Number.Target),Number.Amount,int32(Number.Kind));
}

void UCombatNumberWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry,DeltaTime);
	if (!PresentationData || !Battle.IsValid() || Battle->IsResultReady()) { ClearNumbers(); return; }
	const double Now=GetWorld()->GetTimeSeconds();
	const float Duration=Bounded(PresentationData->Duration,.2f,5.f,1.1f);
	Entries.RemoveAll([&](const FEntry& Entry) { return Now-Entry.StartedAt>=Duration; });
	for (FEntry& Entry:Entries)
	{
		if (AActor* Target=Entry.Target.Get())
		{
			Entry.LastWorldPosition=UCombatantVisualData::ResolveAnchor(Target,ECombatantAnchor::DamageNumber);
		}
	}
	if (Entries.IsEmpty()) { ClearNumbers(); }
}

int32 UCombatNumberWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer=Super::NativePaint(Args,Geometry,Culling,Elements,Layer,Style,bEnabled);
	const APlayerController* PC=GetOwningPlayer();
	if (!PresentationData || !PC || !GetWorld()) { return Layer; }
	int32 Width=0, Height=0; PC->GetViewportSize(Width,Height);
	if (Width<=0 || Height<=0) { return Layer; }
	const FVector2D Size=Geometry.GetLocalSize();
	const auto* Data=PresentationData.Get();
	const float Duration=Bounded(Data->Duration,.2f,5.f,1.1f);
	for (const FEntry& Entry:Entries)
	{
		FVector2D Screen;
		if (!PC->ProjectWorldLocationToScreen(Entry.LastWorldPosition,Screen,true)) { continue; }
		const float Age=FMath::Max(0.f,float(GetWorld()->GetTimeSeconds()-Entry.StartedAt));
		const float Progress=FMath::Clamp(Age/Duration,0.f,1.f);
		const float Fade=FMath::Min(Bounded(Data->FadeDuration,.01f,2.f,.35f),Duration);
		const float Alpha=FMath::Clamp((Duration-Age)/Fade,0.f,1.f)*Style.GetColorAndOpacityTint().A;
		const float Pop=FMath::Lerp(Bounded(Data->InitialScale,1.f,2.f,1.2f),1.f,FMath::Clamp(Age/Bounded(Data->PopDuration,.01f,.5f,.12f),0.f,1.f));
		FSlateFontInfo Font=Data->Font;
		if (!Font.HasValidFont()) { Font=GetDefault<UTextBlock>()->GetFont(); Font.Size=54; }
		Font.Size=Bounded(Font.Size,12.f,160.f,54.f)*Pop*(Entry.Kind==ECombatNumberKind::BonusDamage?Bounded(Data->BonusScale,.5f,2.f,1.12f):1.f);
		FLinearColor Color=Entry.Kind==ECombatNumberKind::Healing?Data->HealingColor:Entry.Kind==ECombatNumberKind::BonusDamage?Data->BonusColor:Data->DamageColor;
		Color.A*=Alpha;
		const FVector2D Extent=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Entry.Label,Font);
		FVector2D Position=Screen*Size/FVector2D(Width,Height)+Data->ScreenOffset+FVector2D(Entry.LateralOffset,-Bounded(Data->RiseDistance,0.f,200.f,50.f)*Progress);
		Position.X=FMath::Clamp(Position.X,Extent.X*.5,FMath::Max(Extent.X*.5,Size.X-Extent.X*.5));
		Position.Y=FMath::Clamp(Position.Y,Extent.Y*.5,FMath::Max(Extent.Y*.5,Size.Y-Extent.Y*.5));
		Position-=Extent*.5;
		FLinearColor Shadow=Data->ShadowColor; Shadow.A*=Alpha;
		FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(Position+Data->ShadowOffset)),Entry.Label,Font,ESlateDrawEffect::None,Shadow);
		FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(Position)),Entry.Label,Font,ESlateDrawEffect::None,Color);
	}
	return Layer;
}
