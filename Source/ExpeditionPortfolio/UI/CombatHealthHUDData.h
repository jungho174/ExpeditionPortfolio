#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "CombatHealthHUDData.generated.h"

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UCombatHealthHUDData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Party",meta=(ClampMin="120")) float PanelWidth=230;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Party",meta=(ClampMin="80")) float PanelHeight=144;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Party",meta=(ClampMin="0")) float PanelGap=12;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Party") FVector2D BottomRightMargin=FVector2D(28,24);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Party") FLinearColor PartyHealthColor=FLinearColor(.65f,.14f,.19f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Party") FLinearColor ActiveColor=FLinearColor(1,.83f,.44f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Party",meta=(ToolTip="Show the portrait, its diamond frame, and the fallback silhouette above each party health panel.")) bool bShowPartyPortrait=true;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fonts") FSlateFontInfo NameFont;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fonts") FSlateFontInfo HealthFont;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Enemy") FVector2D EnemyBarSize=FVector2D(154,9);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Enemy") FLinearColor EnemyHealthColor=FLinearColor(.7f,.12f,.16f);
};
