#include "UI/CombatCompositionEditorLibrary.h"
#if WITH_EDITOR
#include "Combat/BattleEnemyActor.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"

namespace
{
	void CopyEditableDefaults(UActorComponent* Source,UActorComponent* Target)
	{
		for (TFieldIterator<FProperty> It(Source->GetClass());It;++It)
		{
			FProperty* Property=*It;
			if (Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_Transient))
			{ Property->CopyCompleteValue_InContainer(Target,Source); }
		}
	}
}
#endif

UActorComponent* UCombatCompositionEditorLibrary::EnsureActionExecutor(UBlueprint* Blueprint,TSubclassOf<UActorComponent> ComponentClass,FName Name)
{
#if WITH_EDITOR
	if (!Blueprint || !Blueprint->SimpleConstructionScript || !ComponentClass || Name.IsNone()) { return nullptr; }
	if (auto* Existing=Blueprint->SimpleConstructionScript->FindSCSNode(Name))
	{ return Existing->ComponentClass.Get()==ComponentClass.Get()?Existing->ComponentTemplate.Get():nullptr; }
	Blueprint->Modify();Blueprint->SimpleConstructionScript->Modify();
	auto* Node=Blueprint->SimpleConstructionScript->CreateNode(ComponentClass,Name);
	Blueprint->SimpleConstructionScript->AddNode(Node);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return Node->ComponentTemplate;
#else
	return nullptr;
#endif
}

bool UCombatCompositionEditorLibrary::SplitEnemyContent(UBlueprint* Legacy,UBlueprint* Species)
{
#if WITH_EDITOR
	if (!Legacy || !Species || Legacy==Species || !Legacy->SimpleConstructionScript || !Species->SimpleConstructionScript) { return false; }
	if (Species->ParentClass==ABattleEnemyActor::StaticClass() && Legacy->ParentClass==Species->GeneratedClass) { return true; }
	// Capture species scene content before either inheritance chain changes.
	TArray<USCS_Node*> SourceNodes;
	for (USCS_Node* Node:Legacy->SimpleConstructionScript->GetAllNodes())
	{ if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>()) { SourceNodes.Add(Node); } }
	const auto* PreviousCharacter=Cast<ACharacter>(Legacy->GeneratedClass->GetDefaultObject());
	if (!PreviousCharacter && SourceNodes.IsEmpty()) { return false; }
	Species->Modify();Species->ParentClass=ABattleEnemyActor::StaticClass();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Species);FKismetEditorUtilities::CompileBlueprint(Species);
	if (Species->Status==BS_Error) { return false; }
	if (PreviousCharacter)
	{
		auto* NewCharacter=CastChecked<ACharacter>(Species->GeneratedClass->GetDefaultObject());
		CopyEditableDefaults(PreviousCharacter->GetMesh(),NewCharacter->GetMesh());
		CopyEditableDefaults(PreviousCharacter->GetCapsuleComponent(),NewCharacter->GetCapsuleComponent());
		CopyEditableDefaults(PreviousCharacter->GetCharacterMovement(),NewCharacter->GetCharacterMovement());
	}
	TMap<USCS_Node*,USCS_Node*> Copies;
	for (USCS_Node* Node:SourceNodes)
	{
		USCS_Node* Copy=Species->SimpleConstructionScript->FindSCSNode(Node->GetVariableName());
		if (!Copy) { Copy=Species->SimpleConstructionScript->CreateNode(Node->ComponentClass,Node->GetVariableName()); }
		CopyEditableDefaults(Node->ComponentTemplate,Copy->ComponentTemplate);Copies.Add(Node,Copy);
	}
	for (USCS_Node* Node:SourceNodes)
	{
		USCS_Node* Copy=Copies.FindChecked(Node);bool bChild=false;
		for (USCS_Node* Parent:SourceNodes)
		{
			if (Parent->GetChildNodes().Contains(Node)) { Copies.FindChecked(Parent)->AddChildNode(Copy);bChild=true;break; }
		}
		if (!bChild) { Species->SimpleConstructionScript->AddNode(Copy); }
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Species);FKismetEditorUtilities::CompileBlueprint(Species);
	if (Species->Status==BS_Error) { return false; }
	Legacy->Modify();
	const auto PreviousNodes=Legacy->SimpleConstructionScript->GetAllNodes();
	for (USCS_Node* Node:PreviousNodes) { Legacy->SimpleConstructionScript->RemoveNode(Node); }
	Legacy->ParentClass=Species->GeneratedClass;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Legacy);FKismetEditorUtilities::CompileBlueprint(Legacy);
	return Legacy->Status!=BS_Error;
#else
	return false;
#endif
}
