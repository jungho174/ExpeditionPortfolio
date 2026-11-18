#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "CombatNumberData.generated.h"

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UCombatNumberData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Appearance") FSlateFontInfo Font;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Appearance") FLinearColor DamageColor=FLinearColor(.96f,.94f,.86f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Appearance") FLinearColor HealingColor=FLinearColor(.24f,1.f,.36f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Appearance") FLinearColor BonusColor=FLinearColor(1.f,.69f,.18f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Appearance", meta=(ClampMin="0.5",ClampMax="2")) float BonusScale=1.12f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Appearance") FLinearColor ShadowColor=FLinearColor(0,0,0,.75f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Appearance") FVector2D ShadowOffset=FVector2D(2,3);
	UPROPERTY(BlueprintReadOnly,Category="Legacy") FName AnchorBone=TEXT("spine_03");
	UPROPERTY(BlueprintReadOnly,Category="Legacy") FVector WorldOffset=FVector(0,0,35);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Position") FVector2D ScreenOffset=FVector2D(0,-25);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Motion", meta=(ClampMin="0.2",ClampMax="5",Units="s")) float Duration=1.1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Motion", meta=(ClampMin="0",ClampMax="200")) float RiseDistance=50;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Motion", meta=(ClampMin="1",ClampMax="2")) float InitialScale=1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Motion", meta=(ClampMin="0.01",ClampMax="0.5",Units="s")) float PopDuration=.12f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers|Motion", meta=(ClampMin="0.01",ClampMax="2",Units="s")) float FadeDuration=.35f;
};
