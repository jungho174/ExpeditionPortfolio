#include "UI/BattleSlowMotionSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

uint64 UBattleSlowMotionSubsystem::Begin(EBattleSlowMotion Effect, float Factor)
{
	if (CurrentId && CurrentEffect == EBattleSlowMotion::Result && Effect == EBattleSlowMotion::Parry) { return 0; }
	if (!CurrentId)
	{
		Settings = GetWorld()->GetWorldSettings(false, false);
		if (!Settings.IsValid()) { return 0; }
		OriginalDilation = Settings->TimeDilation;
	}
	if (!Settings.IsValid()) { return 0; }
	CurrentEffect = Effect;
	CurrentId = NextId++;
	Settings->SetTimeDilation(OriginalDilation * Factor);
	return CurrentId;
}

void UBattleSlowMotionSubsystem::End(uint64 Id)
{
	if (!Id || Id != CurrentId) { return; }
	if (Settings.IsValid()) { Settings->SetTimeDilation(OriginalDilation); }
	Settings.Reset();
	CurrentId = 0;
}

void UBattleSlowMotionSubsystem::Deinitialize()
{
	End(CurrentId);
	Super::Deinitialize();
}
