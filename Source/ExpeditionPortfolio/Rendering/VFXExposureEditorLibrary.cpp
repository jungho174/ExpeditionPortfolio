#include "Rendering/VFXExposureEditorLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionEyeAdaptation.h"
#include "Materials/MaterialExpressionEyeAdaptationInverse.h"
#endif

namespace
{
bool IsProjectAsset(const UObject* Object)
{
    return Object && Object->GetPathName().StartsWith(TEXT("/Game/ExpeditionPortfolio/"));
}
TArray<UMaterialInterface*> RendererMaterials(UNiagaraRendererProperties* Renderer)
{
    TArray<UMaterialInterface*> Materials;
    if (auto* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer)) { Materials.Add(Sprite->Material); }
    else if (auto* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer)) { Materials.Add(Ribbon->Material); }
    else if (auto* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
    {
        int32 Count = Mesh->bOverrideMaterials ? Mesh->OverrideMaterials.Num() : 0;
        for (const auto& Entry : Mesh->Meshes) { if (Entry.Mesh) { Count = FMath::Max(Count, Entry.Mesh->GetStaticMaterials().Num()); } }
        for (int32 Index = 0; Index < Count; ++Index)
        {
            UMaterialInterface* Material = Mesh->bOverrideMaterials && Mesh->OverrideMaterials.IsValidIndex(Index) ? Mesh->OverrideMaterials[Index].ExplicitMat.Get() : nullptr;
            if (!Material && Mesh->Meshes.Num() && Mesh->Meshes[0].Mesh) { Material = Mesh->Meshes[0].Mesh->GetMaterial(Index); }
            Materials.Add(Material);
        }
    }
    return Materials;
}
}

TArray<FVFXMaterialSlot> UVFXExposureEditorLibrary::GetVFXMaterialSlots(UNiagaraSystem* System)
{
    TArray<FVFXMaterialSlot> Slots;
#if WITH_EDITOR
    if (!System) { return Slots; }
    for (int32 E = 0; E < System->GetNumEmitters(); ++E)
    {
        const auto& Handle = System->GetEmitterHandle(E);
        const auto* Data = Handle.GetEmitterData();
        if (!Data) { continue; }
        const auto& Renderers = Data->GetRenderers();
        for (int32 R = 0; R < Renderers.Num(); ++R)
        {
            const auto Materials = RendererMaterials(Renderers[R]);
            for (int32 M = 0; M < Materials.Num(); ++M)
            {
                if (!Materials[M]) { continue; }
                auto& Slot = Slots.AddDefaulted_GetRef();
                Slot.EmitterIndex = E; Slot.RendererIndex = R; Slot.MaterialSlot = M;
                Slot.EmitterName = Handle.GetName().ToString(); Slot.RendererType = Renderers[R]->GetClass()->GetName(); Slot.Material = Materials[M];
            }
        }
    }
#endif
    return Slots;
}

bool UVFXExposureEditorLibrary::SetVFXMaterialSlot(UNiagaraSystem* System, int32 EmitterIndex, int32 RendererIndex, int32 MaterialSlot, UMaterialInterface* Material)
{
#if WITH_EDITOR
    if (!IsProjectAsset(System) || !IsProjectAsset(Material) || EmitterIndex < 0 || EmitterIndex >= System->GetNumEmitters()) { return false; }
    auto& Handle = System->GetEmitterHandle(EmitterIndex);
    auto* Data = Handle.GetEmitterData();
    if (!Data || !Data->GetRenderers().IsValidIndex(RendererIndex)) { return false; }
    auto* Renderer = Data->GetRenderers()[RendererIndex];
    if (Renderer->GetOutermost() != System->GetOutermost()) { return false; }
    const auto Materials = RendererMaterials(Renderer);
    if (!Materials.IsValidIndex(MaterialSlot)) { return false; }
    System->Modify(); Renderer->Modify();
    if (auto* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer)) { Sprite->Material = Material; }
    else if (auto* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer)) { Ribbon->Material = Material; }
    else if (auto* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
    {
        // A shared slot must mean the same thing for every static mesh in this renderer.
        if (!Mesh->bOverrideMaterials)
        {
            for (const auto& Entry : Mesh->Meshes)
            {
                if (Entry.Mesh && Entry.Mesh->GetMaterial(MaterialSlot) != Materials[MaterialSlot]) { return false; }
            }
        }
        if (Mesh->OverrideMaterials.Num() < Materials.Num()) { Mesh->OverrideMaterials.SetNum(Materials.Num()); }
        for (int32 M = 0; M < Materials.Num(); ++M)
        {
            if (!Mesh->bOverrideMaterials) { Mesh->OverrideMaterials[M].ExplicitMat = Materials[M]; }
        }
        Mesh->bOverrideMaterials = true;
        Mesh->OverrideMaterials[MaterialSlot].ExplicitMat = Material;
    }
    else { return false; }
    System->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

FString UVFXExposureEditorLibrary::GetMaterialExposureStatus(UMaterialInterface* Interface)
{
#if WITH_EDITOR
    UMaterial* Material = Interface ? Interface->GetMaterial() : nullptr;
    if (!Material) { return TEXT("missing"); }
    TArray<UMaterialExpressionMaterialFunctionCall*> Calls;
    Material->GetAllExpressionsInMaterialAndFunctionsOfType(Calls);
    for (auto* Call : Calls)
    {
        if (Call->MaterialFunction && Call->MaterialFunction->GetName() == TEXT("MF_VFXExposure")) { return TEXT("common"); }
    }
    TArray<UMaterialExpressionEyeAdaptationInverse*> Inverses;
    Material->GetAllExpressionsInMaterialAndFunctionsOfType(Inverses);
    if (Inverses.Num()) { return TEXT("existing-inverse"); }
    TArray<UMaterialExpressionEyeAdaptation*> Adaptations;
    Material->GetAllExpressionsInMaterialAndFunctionsOfType(Adaptations);
    if (Adaptations.Num()) { return TEXT("review-existing-exposure"); }
    if (Material->GetEditorOnlyData() && Material->GetEditorOnlyData()->EmissiveColor.Expression) { return TEXT("uncompensated"); }
    if (Material->GetEditorOnlyData() && Material->GetEditorOnlyData()->MaterialAttributes.Expression)
    {
        auto* Attributes = Cast<UMaterialExpressionMakeMaterialAttributes>(Material->GetEditorOnlyData()->MaterialAttributes.Expression);
        if (Attributes) { return Attributes->EmissiveColor.Expression ? TEXT("uncompensated") : TEXT("no-direct-emissive"); }
        return TEXT("review-material-attributes");
    }
    return TEXT("no-direct-emissive");
#else
    return TEXT("editor-only");
#endif
}

bool UVFXExposureEditorLibrary::FinalizeVFXMaterials(UNiagaraSystem* System)
{
#if WITH_EDITOR
    if (!IsProjectAsset(System)) { return false; }
    System->PostEditChange();
    System->RequestCompile(true);
    System->WaitForCompilationComplete(false, false);
    System->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

bool UVFXExposureEditorLibrary::InitializeExposureFunction(UMaterialFunction* Function)
{
#if WITH_EDITOR
    if (!IsProjectAsset(Function)) { return false; }
    if (!Function->GetExpressionCollection().Expressions.IsEmpty())
    {
        TArray<UMaterialExpressionEyeAdaptationInverse*> Nodes;
        Function->GetAllExpressionsOfType(Nodes);
        return Nodes.Num() == 1;
    }
    auto Input = [Function](const TCHAR* Name, EFunctionInputType Type, float Default, int32 Order)
    {
        auto* Node = CastChecked<UMaterialExpressionFunctionInput>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Function, UMaterialExpressionFunctionInput::StaticClass(), -700, Order * 160));
        Node->InputName = Name; Node->InputType = Type; Node->PreviewValue = FVector4f(Default, Default, Default, 0);
        Node->bUsePreviewValueAsDefault = true; Node->SortPriority = Order;
        return Node;
    };
    auto* Emissive = Input(TEXT("Emissive"), FunctionInput_Vector3, 1, 0);
    auto* Strength = Input(TEXT("Strength"), FunctionInput_Scalar, 1, 1);
    auto* Reference = Input(TEXT("ReferenceExposure"), FunctionInput_Scalar, FMath::Exp2(-1.5f), 2);
    auto* Brightness = Input(TEXT("Brightness"), FunctionInput_Scalar, 1, 3);
    auto* Scale = CastChecked<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Function, UMaterialExpressionMultiply::StaticClass(), -400, 0));
    Scale->A.Connect(0, Emissive); Scale->B.Connect(0, Brightness);
    auto* Inverse = CastChecked<UMaterialExpressionEyeAdaptationInverse>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Function, UMaterialExpressionEyeAdaptationInverse::StaticClass(), -150, 0));
    Inverse->LightValueInput.Connect(0, Scale); Inverse->AlphaInput.Connect(0, Strength);
    auto* Power = CastChecked<UMaterialExpressionPower>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Function, UMaterialExpressionPower::StaticClass(), -150, 250));
    Power->Base.Connect(0, Reference); Power->Exponent.Connect(0, Strength);
    auto* Result = CastChecked<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Function, UMaterialExpressionMultiply::StaticClass(), 100, 0));
    Result->A.Connect(0, Inverse); Result->B.Connect(0, Power);
    auto* Output = CastChecked<UMaterialExpressionFunctionOutput>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Function, UMaterialExpressionFunctionOutput::StaticClass(), 350, 0));
    Output->OutputName = TEXT("Emissive"); Output->A.Connect(0, Result);
    Function->Description = TEXT("Preserve reference appearance while compensating camera exposure. Strength 0 bypasses; 1 cancels exposure. Smoke may use a lower strength.");
    UMaterialEditingLibrary::UpdateMaterialFunction(Function);
    Function->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

FString UVFXExposureEditorLibrary::AddMaterialExposure(UMaterial* Material, UMaterialFunction* Function)
{
#if WITH_EDITOR
    if (!IsProjectAsset(Material) || !IsProjectAsset(Function)) { return TEXT("protected-source"); }
    const FString Status = GetMaterialExposureStatus(Material);
    if (Status != TEXT("uncompensated")) { return Status; }
    FExpressionInput* EmissiveOutput = &Material->GetEditorOnlyData()->EmissiveColor;
    if (!EmissiveOutput->Expression)
    {
        auto* Attributes = Cast<UMaterialExpressionMakeMaterialAttributes>(Material->GetEditorOnlyData()->MaterialAttributes.Expression);
        if (!Attributes) { return TEXT("review-material-attributes"); }
        EmissiveOutput = &Attributes->EmissiveColor;
    }
    const FExpressionInput Original = *EmissiveOutput;
    Material->Modify();
    auto* Call = CastChecked<UMaterialExpressionMaterialFunctionCall>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionMaterialFunctionCall::StaticClass(), 600, 0));
    if (!Call->SetMaterialFunction(Function)) { return TEXT("function-error"); }
    auto Parameter = [Material](const TCHAR* Name, float Default, int32 Y)
    {
        auto* Node = CastChecked<UMaterialExpressionScalarParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), 300, Y));
        Node->ParameterName = Name; Node->DefaultValue = Default; Node->Group = TEXT("VFX Exposure");
        return Node;
    };
    auto* Strength = Parameter(TEXT("VFXExposureStrength"), 1, 200);
    auto* Reference = Parameter(TEXT("VFXReferenceExposure"), FMath::Exp2(-1.5f), 350);
    auto* Brightness = Parameter(TEXT("VFXBrightness"), 1, 500);
    for (auto& Input : Call->FunctionInputs)
    {
        const FName Name = Input.ExpressionInput->InputName;
        if (Name == TEXT("Emissive")) { Input.Input = Original; }
        else if (Name == TEXT("Strength")) { Input.Input.Connect(0, Strength); }
        else if (Name == TEXT("ReferenceExposure")) { Input.Input.Connect(0, Reference); }
        else if (Name == TEXT("Brightness")) { Input.Input.Connect(0, Brightness); }
    }
    EmissiveOutput->Connect(0, Call);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->MarkPackageDirty();
    return TEXT("added");
#else
    return TEXT("editor-only");
#endif
}
