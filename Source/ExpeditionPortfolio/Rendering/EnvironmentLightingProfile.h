#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Scene.h"
#include "EnvironmentLightingProfile.generated.h"

// Environment settings belong to a level, independently of character/action data.
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UEnvironmentLightingProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exposure", meta=(ValidEnumValues="AEM_Histogram,AEM_Basic")) TEnumAsByte<EAutoExposureMethod> MeteringMethod = AEM_Histogram;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exposure") float MinEV100 = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exposure") float MaxEV100 = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exposure") float ExposureBias = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lighting", meta=(ClampMin="0", Units="lux")) float SunIntensity = 17.578125f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lighting", meta=(ClampMin="0", Units="lux")) float FillIntensity = 2.44140625f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lighting", meta=(ClampMin="0")) float SkyIntensity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Post Process", meta=(ClampMin="0")) float BloomIntensity = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Post Process", meta=(ClampMin="0",ClampMax="1")) float HighlightContrast = .8f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Post Process", meta=(ClampMin="0",ClampMax="1")) float ShadowContrast = .8f;

    UFUNCTION(BlueprintPure, Category="Lighting") bool IsValidProfile() const;
};
