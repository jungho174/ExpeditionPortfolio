#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VFXExposureEditorLibrary.generated.h"
class UMaterial;
class UMaterialInterface;
class UMaterialFunction;
class UNiagaraSystem;

USTRUCT(BlueprintType)
struct FVFXMaterialSlot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 EmitterIndex = 0;
    UPROPERTY(BlueprintReadOnly) int32 RendererIndex = 0;
    UPROPERTY(BlueprintReadOnly) int32 MaterialSlot = 0;
    UPROPERTY(BlueprintReadOnly) FString EmitterName;
    UPROPERTY(BlueprintReadOnly) FString RendererType;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UMaterialInterface> Material;
};

// Authoring-only graph/renderer access, including renderer types not exposed to Python.
UCLASS()
class EXPEDITIONPORTFOLIO_API UVFXExposureEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Rendering|Editor") static bool InitializeExposureFunction(UMaterialFunction* Function);
    UFUNCTION(BlueprintCallable, Category="Rendering|Editor") static FString AddMaterialExposure(UMaterial* Material, UMaterialFunction* Function);
    UFUNCTION(BlueprintPure, Category="Rendering|Editor") static FString GetMaterialExposureStatus(UMaterialInterface* Material);
    UFUNCTION(BlueprintPure, Category="Rendering|Editor") static TArray<FVFXMaterialSlot> GetVFXMaterialSlots(UNiagaraSystem* System);
    UFUNCTION(BlueprintCallable, Category="Rendering|Editor") static bool SetVFXMaterialSlot(UNiagaraSystem* System, int32 EmitterIndex, int32 RendererIndex, int32 MaterialSlot, UMaterialInterface* Material);
    UFUNCTION(BlueprintCallable, Category="Rendering|Editor") static bool FinalizeVFXMaterials(UNiagaraSystem* System);
};
