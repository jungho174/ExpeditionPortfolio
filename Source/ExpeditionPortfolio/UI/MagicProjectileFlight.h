#pragma once

#include "CoreMinimal.h"

// 한 번의 시전에서 발사 시점과 도착 지점을 고정해 명중을 한 번만 처리한다.
class FMagicProjectileFlight
{
public:
	bool Launch(const FVector& Origin, const FVector& Destination, float Seconds, uint64 Frame)
	{
		if (HasLaunched() || Seconds <= 0.0f) { return false; }
		Start = Origin;
		End = Destination;
		Duration = Seconds;
		LaunchFrame = Frame;
		State = EState::Flying;
		return true;
	}

	bool CanAdvance(uint64 Frame) const { return IsFlying() && Frame != LaunchFrame; }
	bool HasLaunched() const { return State != EState::Ready; }
	bool IsFlying() const { return State == EState::Flying; }
	bool HasArrived() const { return State == EState::Arrived; }
	const FVector& GetEnd() const { return End; }
	FRotator GetFacing() const { return (End - Start).Rotation(); }

	FVector Advance(float DeltaTime)
	{
		Elapsed += FMath::Max(0.0f, DeltaTime);
		const float Alpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
		if (Alpha >= 1.0f) { State = EState::Arrived; }
		return FMath::Lerp(Start, End, Alpha);
	}

private:
	enum class EState : uint8 { Ready, Flying, Arrived };
	EState State = EState::Ready;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float Elapsed = 0.0f;
	float Duration = 0.0f;
	uint64 LaunchFrame = 0;
};
