#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VFXValidationActor.generated.h"
class UNiagaraSystem;
class UNiagaraComponent;
class AEnvironmentLightingRig;
class UEnvironmentLightingProfile;

USTRUCT(BlueprintType)
struct FVFXValidationEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UNiagaraSystem> Effect;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform Transform;
};

// A repeatable preview fixture for the validation map. Timers/input only, no Actor Tick.
UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API AVFXValidationActor : public AActor
{
    GENERATED_BODY()
public:
    AVFXValidationActor();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Preview") TObjectPtr<AEnvironmentLightingRig> LightingRig;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview") TArray<TObjectPtr<UEnvironmentLightingProfile>> LightingProfiles;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview") TArray<FVFXValidationEntry> Effects;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview", meta=(ClampMin="0")) int32 SelectedEffect = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview") TObjectPtr<UNiagaraComponent> EffectComponent;
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Preview") void ReplayEffect();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Preview") void NextEffect();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Preview") void StopPreview();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Preview") void UseOutdoorProfile();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Preview") void UseDimProfile();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Preview") void UseHDRProfile();
    UFUNCTION(BlueprintCallable, Category="Preview") bool SelectLightingProfile(int32 Index);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    FTimerHandle ReplayTimer;
    int32 CurrentProfile = 0;
    void ShowStatus() const;
};
