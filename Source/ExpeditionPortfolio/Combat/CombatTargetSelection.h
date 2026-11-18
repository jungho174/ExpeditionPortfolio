#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatantComponent.h"
#include "GameFramework/Actor.h"

// Stable left-to-right order captured before the target camera moves.
struct FCombatTargetSelection
{
	TArray<TWeakObjectPtr<AActor>> Candidates;
	int32 Index = 0;
	static bool IsEligible(AActor* Actor)
	{
		const auto* Combatant = IsValid(Actor) ? Actor->FindComponentByClass<UCombatantComponent>() : nullptr;
		return Combatant && !Combatant->IsDefeated();
	}
	void Reset() { Candidates.Reset(); Index = 0; }
	AActor* Get() const { return Candidates.IsValidIndex(Index) && IsEligible(Candidates[Index].Get()) ? Candidates[Index].Get() : nullptr; }
	void Build(const TArray<AActor*>& Actors, FVector Right)
	{
		Reset();
		for (AActor* Actor : Actors) { if (IsEligible(Actor)) { Candidates.AddUnique(Actor); } }
		Candidates.StableSort([Right](const TWeakObjectPtr<AActor>& A, const TWeakObjectPtr<AActor>& B)
		{
			return FVector::DotProduct(A->GetActorLocation(), Right) < FVector::DotProduct(B->GetActorLocation(), Right);
		});
	}
	void Move(int32 Direction)
	{
		AActor* Previous = Get();
		Candidates.RemoveAll([](const TWeakObjectPtr<AActor>& Actor) { return !IsEligible(Actor.Get()); });
		if (Candidates.IsEmpty()) { Index = 0; return; }
		const int32 PreviousIndex = Candidates.IndexOfByKey(Previous);
		Index = PreviousIndex == INDEX_NONE ? FMath::Min(Index, Candidates.Num()-1) : PreviousIndex;
		if (Direction) { Index = (Index + (Direction > 0 ? 1 : Candidates.Num()-1)) % Candidates.Num(); }
	}
};
