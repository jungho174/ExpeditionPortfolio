#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleSlowMotionSubsystem.generated.h"

class AWorldSettings;

enum class EBattleSlowMotion : uint8 { Parry, Result };

UCLASS()
class UBattleSlowMotionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	// 결과 연출이 패링을 이어받아도 최초 연출 직전의 월드 배속을 기준으로 삼는다.
	uint64 Begin(EBattleSlowMotion Effect, float Factor);
	// 현재 연출의 번호가 일치할 때만 배속을 복원한다.
	void End(uint64 Id);
	virtual void Deinitialize() override;

private:
	TWeakObjectPtr<AWorldSettings> Settings;
	float OriginalDilation = 1.0f;
	EBattleSlowMotion CurrentEffect = EBattleSlowMotion::Parry;
	uint64 CurrentId = 0;
	uint64 NextId = 1;
};
