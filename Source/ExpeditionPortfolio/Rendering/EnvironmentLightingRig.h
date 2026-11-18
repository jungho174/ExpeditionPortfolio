#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnvironmentLightingRig.generated.h"

class UEnvironmentLightingProfile;
class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;

// References preserve existing authored lights, directions, colors and level layout.
UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API AEnvironmentLightingRig : public AActor
{
    GENERATED_BODY()
public:
    AEnvironmentLightingRig();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lighting") TObjectPtr<UEnvironmentLightingProfile> Profile;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Lighting|Actors") TObjectPtr<ADirectionalLight> SunLight;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Lighting|Actors") TObjectPtr<ADirectionalLight> FillLight;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Lighting|Actors") TObjectPtr<ASkyLight> SkyLight;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Lighting|Actors") TObjectPtr<APostProcessVolume> ExposureVolume;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lighting") bool bApplyInEditor = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lighting") bool bApplyAtBeginPlay = true;
    UPROPERTY(VisibleAnywhere, Transient, Category="Lighting|Validation") TArray<FString> ValidationMessages;

    UFUNCTION(BlueprintCallable, Category="Lighting") bool ApplyProfile();
    UFUNCTION(BlueprintCallable, Category="Lighting") bool SetLightingProfile(UEnvironmentLightingProfile* InProfile);
    UFUNCTION(CallInEditor, Category="Lighting") void ApplyLightingProfile();
    UFUNCTION(CallInEditor, Category="Lighting") void ValidateLighting();
    UFUNCTION(BlueprintPure, Category="Lighting") TArray<FString> GetValidationIssues() const;
    virtual void OnConstruction(const FTransform& Transform) override;
protected:
    virtual void BeginPlay() override;
};
