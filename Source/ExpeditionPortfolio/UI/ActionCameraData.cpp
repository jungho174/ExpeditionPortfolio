#include "UI/ActionCameraData.h"
#include "Combat/CharacterActionData.h"
const FBattleCameraShot& UActionCameraData::ResolveTargetShot(const UCharacterActionData* Action,const FBattleCameraShot& CommonDefault)
{
	const auto* Data=Action?Action->CameraData.Get():nullptr;
	return Data && Data->bOverrideTargetSelection?Data->TargetSelectionShot:CommonDefault;
}
const FBattleCameraShot& UActionCameraData::ResolveShot(const UCharacterActionData* Action,bool bPreparation,const FBattleCameraShot& CommonDefault)
{
	const UActionCameraData* Data=Action?Action->CameraData.Get():nullptr;
	if (!Data) { return CommonDefault; }
	if (bPreparation) { return Data->bOverridePreparation?Data->PreparationShot:CommonDefault; }
	return Data->bOverrideExecution?Data->ExecutionShot:CommonDefault;
}
