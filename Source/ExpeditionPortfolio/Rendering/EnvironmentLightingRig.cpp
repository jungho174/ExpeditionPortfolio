#include "Rendering/EnvironmentLightingRig.h"
#include "Rendering/EnvironmentLightingProfile.h"
#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"

AEnvironmentLightingRig::AEnvironmentLightingRig()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AEnvironmentLightingRig::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (bApplyInEditor && Profile && SunLight && FillLight && SkyLight && ExposureVolume) { ApplyProfile(); }
}

void AEnvironmentLightingRig::BeginPlay()
{
    Super::BeginPlay();
    if (bApplyAtBeginPlay) { ApplyProfile(); }
}

bool AEnvironmentLightingRig::ApplyProfile()
{
    if (!Profile || !Profile->IsValidProfile() || !SunLight || !FillLight || SunLight == FillLight || !SkyLight || !ExposureVolume
        || SunLight->GetWorld() != GetWorld() || FillLight->GetWorld() != GetWorld()
        || SkyLight->GetWorld() != GetWorld() || ExposureVolume->GetWorld() != GetWorld())
    {
        UE_LOG(LogTemp, Warning, TEXT("Lighting profile was not applied: missing/invalid profile or level light references (%s)."), *GetName());
        return false;
    }
    SunLight->GetLightComponent()->SetIntensity(Profile->SunIntensity);
    FillLight->GetLightComponent()->SetIntensity(Profile->FillIntensity);
    SkyLight->GetLightComponent()->SetIntensity(Profile->SkyIntensity);
    FPostProcessSettings& Settings = ExposureVolume->Settings;
    Settings.bOverride_AutoExposureMethod = true;
    Settings.AutoExposureMethod = Profile->MeteringMethod;
    Settings.bOverride_AutoExposureMinBrightness = true; Settings.AutoExposureMinBrightness = Profile->MinEV100;
    Settings.bOverride_AutoExposureMaxBrightness = true; Settings.AutoExposureMaxBrightness = Profile->MaxEV100;
    Settings.bOverride_AutoExposureBias = true; Settings.AutoExposureBias = Profile->ExposureBias;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true; Settings.AutoExposureApplyPhysicalCameraExposure = false;
    Settings.bOverride_BloomIntensity = true; Settings.BloomIntensity = Profile->BloomIntensity;
    Settings.bOverride_LocalExposureHighlightContrastScale = true; Settings.LocalExposureHighlightContrastScale = Profile->HighlightContrast;
    Settings.bOverride_LocalExposureShadowContrastScale = true; Settings.LocalExposureShadowContrastScale = Profile->ShadowContrast;
    ExposureVolume->bUnbound = true;
    return true;
}

void AEnvironmentLightingRig::ApplyLightingProfile() { ApplyProfile(); }

bool AEnvironmentLightingRig::SetLightingProfile(UEnvironmentLightingProfile* InProfile)
{
    if (!InProfile || !InProfile->IsValidProfile()) { return false; }
    Profile = InProfile;
    return ApplyProfile();
}

TArray<FString> AEnvironmentLightingRig::GetValidationIssues() const
{
    TArray<FString> Issues;
    if (!Profile) { Issues.Add(TEXT("Select a Lighting Profile.")); }
    else if (!Profile->IsValidProfile()) { Issues.Add(TEXT("Invalid profile: use Histogram/Basic, finite non-negative light values, and Min EV100 <= Max EV100.")); }
    if (!SunLight || !FillLight || !SkyLight || !ExposureVolume) { Issues.Add(TEXT("Assign Sun, Fill, Sky and Exposure actors from this level.")); }
    if (SunLight && SunLight == FillLight) { Issues.Add(TEXT("Sun and Fill must reference different lights.")); }
    if (!GetWorld()) { return Issues; }
    if ((SunLight && SunLight->GetWorld() != GetWorld()) || (FillLight && FillLight->GetWorld() != GetWorld())
        || (SkyLight && SkyLight->GetWorld() != GetWorld()) || (ExposureVolume && ExposureVolume->GetWorld() != GetWorld()))
    { Issues.Add(TEXT("Lighting actor references must belong to this world.")); }
    int32 Rigs = 0;
    for (TActorIterator<AEnvironmentLightingRig> It(GetWorld()); It; ++It) { ++Rigs; }
    if (Rigs > 1) { Issues.Add(TEXT("Multiple lighting rigs in this world can overwrite each other's settings.")); }
    for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
    {
        const auto* Volume = *It;
        if (Volume == ExposureVolume || !Volume->bEnabled || Volume->BlendWeight <= 0) { continue; }
        const auto& S = Volume->Settings;
        if (S.bOverride_AutoExposureMethod || S.bOverride_AutoExposureMinBrightness || S.bOverride_AutoExposureMaxBrightness || S.bOverride_AutoExposureBias)
        { Issues.Add(FString::Printf(TEXT("Exposure override on another Post Process Volume: %s (Priority %.1f)."), *Volume->GetName(), Volume->Priority)); }
    }
    for (TObjectIterator<UCameraComponent> It; It; ++It)
    {
        if (It->GetWorld() != GetWorld() || It->PostProcessBlendWeight <= 0) { continue; }
        const auto& S = It->PostProcessSettings;
        if (S.bOverride_AutoExposureMethod || S.bOverride_AutoExposureMinBrightness || S.bOverride_AutoExposureMaxBrightness || S.bOverride_AutoExposureBias)
        { Issues.Add(FString::Printf(TEXT("Camera overrides level exposure: %s."), *It->GetPathName())); }
    }
    return Issues;
}

void AEnvironmentLightingRig::ValidateLighting()
{
    ValidationMessages = GetValidationIssues();
    for (const FString& Message : ValidationMessages) { UE_LOG(LogTemp, Warning, TEXT("Lighting validation: %s"), *Message); }
    if (ValidationMessages.IsEmpty()) { UE_LOG(LogTemp, Log, TEXT("Lighting validation passed: %s"), *GetName()); }
}
