#include "Rendering/VFXValidationActor.h"
#include "Rendering/EnvironmentLightingRig.h"
#include "Rendering/EnvironmentLightingProfile.h"
#include "NiagaraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"

AVFXValidationActor::AVFXValidationActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    EffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EffectPreview"));
    EffectComponent->SetupAttachment(GetRootComponent());
    EffectComponent->SetAutoActivate(false);
}

void AVFXValidationActor::BeginPlay()
{
    Super::BeginPlay();
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        EnableInput(PC);
        InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ThisClass::UseOutdoorProfile);
        InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::UseDimProfile);
        InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ThisClass::UseHDRProfile);
        InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ThisClass::ReplayEffect);
        InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ThisClass::NextEffect);
    }
    GetWorld()->GetTimerManager().SetTimer(ReplayTimer, this, &ThisClass::ReplayEffect, 4.f, true, .5f);
    ShowStatus();
}

void AVFXValidationActor::ReplayEffect()
{
    if (!Effects.IsValidIndex(SelectedEffect) || !Effects[SelectedEffect].Effect) { return; }
    EffectComponent->DeactivateImmediate();
    EffectComponent->SetAsset(Effects[SelectedEffect].Effect);
    EffectComponent->SetRelativeTransform(Effects[SelectedEffect].Transform);
    EffectComponent->Activate(true);
    ShowStatus();
}
void AVFXValidationActor::NextEffect()
{
    if (Effects.IsEmpty()) { return; }
    SelectedEffect = (SelectedEffect + 1) % Effects.Num();
    ReplayEffect();
}
void AVFXValidationActor::StopPreview()
{
    EffectComponent->DeactivateImmediate();
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReplayTimer); }
}
bool AVFXValidationActor::SelectLightingProfile(int32 Index)
{
    if (!LightingRig || !LightingProfiles.IsValidIndex(Index) || !LightingProfiles[Index]) { return false; }
    if (!LightingRig->SetLightingProfile(LightingProfiles[Index])) { return false; }
    CurrentProfile = Index;
    ShowStatus();
    return true;
}
void AVFXValidationActor::UseOutdoorProfile() { SelectLightingProfile(0); }
void AVFXValidationActor::UseDimProfile() { SelectLightingProfile(1); }
void AVFXValidationActor::UseHDRProfile() { SelectLightingProfile(2); }
void AVFXValidationActor::ShowStatus() const
{
    if (GEngine && GetWorld() && GetWorld()->IsGameWorld())
    {
        const FString EffectName = Effects.IsValidIndex(SelectedEffect) ? Effects[SelectedEffect].Name.ToString() : TEXT("No effect assigned");
        GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 30.f, FColor::Cyan,
            FString::Printf(TEXT("VFX Validation | 1 Outdoor / 2 Dim / 3 HDR | F Replay / N Next | Profile %d | %s"), CurrentProfile + 1, *EffectName));
    }
}
void AVFXValidationActor::EndPlay(const EEndPlayReason::Type Reason) { StopPreview(); Super::EndPlay(Reason); }
