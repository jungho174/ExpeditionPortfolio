#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

// A sound belongs to an event that already happened. Its tail/delay may outlive the action.
// Snapshot settings and location; never retain actors or play across world/PIE teardown.
namespace CombatSoundPlayback
{
	inline void Play(AActor* Owner, USoundBase* Sound, FVector Location, float Volume, float Delay, float StartTime, bool bUI = false)
	{
		if (!IsValid(Owner) || !Sound || !Owner->GetWorld() || !FMath::IsFinite(Volume)
			|| !FMath::IsFinite(Delay) || !FMath::IsFinite(StartTime)) { return; }
		const TWeakObjectPtr<USoundBase> WeakSound(Sound);
		const TWeakObjectPtr<UWorld> World(Owner->GetWorld());
		auto Emit = [Owner, World, WeakSound, Location, Volume, StartTime, bUI]()
		{
			if (!World.IsValid() || !WeakSound.IsValid() || Owner->IsActorBeingDestroyed()
				|| !Owner->HasActorBegunPlay() || Owner->GetWorld() != World.Get()) { return; }
			auto* Audio = bUI ? UGameplayStatics::SpawnSound2D(Owner, WeakSound.Get(), FMath::Clamp(Volume, 0.f, 2.f), 1.f, FMath::Max(0.f, StartTime))
				: UGameplayStatics::SpawnSoundAtLocation(Owner, WeakSound.Get(), Location, FRotator::ZeroRotator,
					FMath::Clamp(Volume, 0.f, 2.f), 1.f, FMath::Max(0.f, StartTime));
			if (Audio)
			{
				UE_LOG(LogTemp, Log, TEXT("Combat sound played: Sound=%s Start=%.3f Position=%s"),
					*GetNameSafe(WeakSound.Get()), StartTime, *Location.ToString());
			}
		};
		if (Delay <= 0) { Emit(); }
		else
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Owner, MoveTemp(Emit)), FMath::Clamp(Delay, 0.f, 5.f), false);
		}
	}
}
