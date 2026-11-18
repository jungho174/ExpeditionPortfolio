#include "Combat/CombatantVisualData.h"
#include "Combat/CombatantComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UCombatantVisualData::UCombatantVisualData()
{
	HealthBar.bBoundsTopFallback=true;
	HealthBar.BoundsTopOffset=24;
}

const FCombatantAnchor& UCombatantVisualData::GetAnchor(ECombatantAnchor Kind) const
{
	switch (Kind)
	{
	case ECombatantAnchor::Aim:return Aim;
	case ECombatantAnchor::DamageNumber:return DamageNumber;
	case ECombatantAnchor::TargetMarker:return TargetMarker;
	case ECombatantAnchor::ParryIntermediate:return ParryIntermediate;
	case ECombatantAnchor::ParryComplete:return ParryComplete;
	case ECombatantAnchor::HealthBar:return HealthBar;
	default:return Camera;
	}
}
UCombatantVisualData* UCombatantVisualData::Find(const AActor* Actor)
{
	const auto* Combatant=IsValid(Actor)?Actor->FindComponentByClass<UCombatantComponent>():nullptr;
	return Combatant?Combatant->VisualData.Get():nullptr;
}
FVector UCombatantVisualData::ResolveAnchor(const AActor* Actor,ECombatantAnchor Kind)
{
	if (!IsValid(Actor)) { return FVector::ZeroVector; }
	const auto* Data=Find(Actor);
	const FCombatantAnchor Anchor=Data?Data->GetAnchor(Kind):FCombatantAnchor();
	const auto* Mesh=Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!Anchor.Socket.IsNone() && Mesh && Mesh->DoesSocketExist(Anchor.Socket))
	{ return Mesh->GetSocketLocation(Anchor.Socket)+Anchor.WorldOffset; }
	if (Anchor.bBoundsTopFallback || (!Data && (Kind==ECombatantAnchor::TargetMarker || Kind==ECombatantAnchor::HealthBar)))
	{
		FVector Origin,Extent;Actor->GetActorBounds(false,Origin,Extent);
		return Origin+FVector(0,0,Extent.Z+Anchor.BoundsTopOffset);
	}
	return Actor->GetActorLocation()+Anchor.WorldOffset;
}
