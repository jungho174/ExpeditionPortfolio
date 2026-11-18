#include "UI/MagicPresentationEditorLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "Combat/BattleCoordinator.h"
#if WITH_EDITOR
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

bool UMagicPresentationEditorLibrary::PrepareProjectileSystem(UNiagaraSystem* System, UNiagaraSystem* ReferenceSystem)
{
#if WITH_EDITOR
	if (!System || !ReferenceSystem || System == ReferenceSystem
		|| !System->GetPathName().StartsWith(TEXT("/Game/ExpeditionPortfolio/"))
		|| System->GetNumEmitters() != ReferenceSystem->GetNumEmitters()) { return false; }
	bool bChanged = false;
	bool bHasLocalCore = false;
	int32 FlightCollisionModules = 0;
	for (int32 Index = 0; Index < System->GetNumEmitters(); ++Index)
	{
		FNiagaraEmitterHandle& Handle = System->GetEmitterHandle(Index);
		const FNiagaraEmitterHandle& Reference = ReferenceSystem->GetEmitterHandle(Index);
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		const FVersionedNiagaraEmitterData* ReferenceData = Reference.GetEmitterData();
		if (!Data || !ReferenceData || Handle.GetName() != Reference.GetName()) { return false; }
		bHasLocalCore |= ReferenceData->bLocalSpace;
		UE_LOG(LogBattleCoordinator, Log, TEXT("Projectile emitter: %s SourceLocalSpace=%d"), *Handle.GetName().ToString(), ReferenceData->bLocalSpace);
		// The component supplies flight motion; all of this small fireball must follow it.
		if (!Data->bLocalSpace)
		{
			System->Modify();
			if (Handle.GetEmitterBase()) { Handle.GetEmitterBase()->Modify(); }
			Data->bLocalSpace = true;
			bChanged = true;
		}
		// The combat flight owns arrival. The source's collision/kill pair can kill the
		// core against the caster's capsule before it ever leaves the hand.
		if (Handle.GetName() == TEXT("Main"))
		{
			UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
			if (!Source || !Source->NodeGraph) { return false; }
			TArray<UNiagaraNodeFunctionCall*> Modules;
			Source->NodeGraph->GetNodesOfClass(Modules);
			for (UNiagaraNodeFunctionCall* Module : Modules)
			{
				if (!Module->FunctionScript) { continue; }
				const FString Name = Module->FunctionScript->GetName();
				if (Name != TEXT("Collision") && Name != TEXT("KillParticles")) { continue; }
				++FlightCollisionModules;
				if (Module->IsNodeEnabled())
				{
					System->Modify();
					FNiagaraStackGraphUtilities::SetModuleIsEnabled(*Module, false);
					bChanged = true;
				}
			}
		}
	}
	if (bChanged)
	{
		System->RequestCompile(true);
		System->WaitForCompilationComplete(false, false);
	}
	return bHasLocalCore && FlightCollisionModules == 2;
#else
	return false;
#endif
}
