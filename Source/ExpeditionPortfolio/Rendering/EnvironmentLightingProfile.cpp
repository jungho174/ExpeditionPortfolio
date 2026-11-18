#include "Rendering/EnvironmentLightingProfile.h"

bool UEnvironmentLightingProfile::IsValidProfile() const
{
    const float Values[] = {MinEV100, MaxEV100, ExposureBias, SunIntensity, FillIntensity, SkyIntensity,
        BloomIntensity, HighlightContrast, ShadowContrast};
    for (float Value : Values) { if (!FMath::IsFinite(Value)) { return false; } }
    return (MeteringMethod == AEM_Histogram || MeteringMethod == AEM_Basic)
        && MinEV100 <= MaxEV100 && SunIntensity >= 0 && FillIntensity >= 0 && SkyIntensity >= 0
        && BloomIntensity >= 0 && HighlightContrast >= 0 && HighlightContrast <= 1 && ShadowContrast >= 0 && ShadowContrast <= 1;
}
