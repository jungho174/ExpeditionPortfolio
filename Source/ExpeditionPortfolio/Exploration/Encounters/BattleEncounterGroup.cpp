#include "Exploration/Encounters/BattleEncounterGroup.h"
#include "Exploration/Encounters/EncounterGroupData.h"
#include "Components/ChildActorComponent.h"
#include "Engine/World.h"

void ABattleEncounterGroup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildPreview();
}

void ABattleEncounterGroup::RebuildPreview()
{
	// Runtime edits must not respawn participants during an active battle.
	if (GetWorld() && GetWorld()->HasBegunPlay()) { return; }
	for (UChildActorComponent* Member:Members) { if (IsValid(Member)) { Member->DestroyComponent(); } }
	Members.Reset();
	if (!Composition || !Composition->IsValidComposition()) { return; }
	for (int32 Index=0;Index<Composition->Members.Num();++Index)
	{
		const auto& Definition=Composition->Members[Index];
		auto* Component=NewObject<UChildActorComponent>(this,NAME_None,RF_Transactional);
		Component->CreationMethod=EComponentCreationMethod::UserConstructionScript;
		Component->SetupAttachment(GetRootComponent());
		Component->SetRelativeTransform(Definition.RelativeTransform);
		Component->SetChildActorClass(Definition.EnemyClass);
		Component->RegisterComponent();
		Members.Add(Component);
	}
}

TArray<AActor*> ABattleEncounterGroup::GetEncounterEnemies() const
{
	TArray<AActor*> Actors;
	for (UChildActorComponent* Member:Members) { if (IsValid(Member) && IsValid(Member->GetChildActor())) { Actors.Add(Member->GetChildActor()); } }
	return Actors;
}
