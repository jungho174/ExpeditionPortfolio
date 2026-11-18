#include "UI/QTEPresentationComponent.h"
#include "UI/QTEPresentationData.h"
#include "UI/CombatSoundPlayback.h"
#include "Combat/ActionQTESubsystem.h"
#include "GameFramework/PlayerController.h"

UQTEPresentationComponent::UQTEPresentationComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UQTEPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	const auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController()) { return; }
	QTE = GetWorld()->GetSubsystem<UActionQTESubsystem>();
	StartHandle = QTE->Started.AddUObject(this, &ThisClass::OnStarted);
	ResultHandle = QTE->StepResolved.AddUObject(this, &ThisClass::OnStepResolved);
	StepStartHandle = QTE->StepStarted.AddUObject(this, &ThisClass::OnStepStarted);
	CancelHandle = QTE->Cancelled.AddUObject(this, &ThisClass::OnCancelled);
}
void UQTEPresentationComponent::ClearTimers()
{
	if (GetWorld()) { for (auto Handle : Timers) { GetWorld()->GetTimerManager().ClearTimer(Handle); } }
	Timers.Reset();
}
void UQTEPresentationComponent::Queue(const FQTESound& Settings)
{
	if (!Settings.Sound || !FMath::IsFinite(Settings.Delay)) { return; }
	if (Settings.Delay <= 0)
	{
		CombatSoundPlayback::Play(GetOwner(), Settings.Sound, FVector::ZeroVector, Settings.Volume, 0, Settings.StartTime, true);
		return;
	}
	const int64 Expected = AudioSession;
	const TWeakObjectPtr<USoundBase> Sound(Settings.Sound);
	const float Volume = Settings.Volume, Start = Settings.StartTime;
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Expected, Sound, Volume, Start]
	{
		if (AudioSession == Expected && Sound.IsValid())
		{
			CombatSoundPlayback::Play(GetOwner(), Sound.Get(), FVector::ZeroVector, Volume, 0, Start, true);
		}
	}), FMath::Clamp(Settings.Delay, 0.f, 5.f), false);
	Timers.Add(Handle);
}
void UQTEPresentationComponent::OnStarted(int64 Session)
{
	ClearTimers(); AudioSession = Session; bResultPlayed = false;
	PlayedSteps.Reset();
	if (PresentationData) { Queue(PresentationData->StartSound); }
}
void UQTEPresentationComponent::OnResolved(int64 Session, EAttackTimingResult Result)
{
	if (Session != AudioSession || bResultPlayed) { return; }
	bResultPlayed = true;
	if (!PresentationData) { return; }
	if (Result == EAttackTimingResult::Success) { Queue(PresentationData->PerfectSound); }
	else if (Result == EAttackTimingResult::Failed) { Queue(PresentationData->RegularSound); }
	else if (Result == EAttackTimingResult::Missed) { Queue(PresentationData->MissSound); }
}
void UQTEPresentationComponent::OnStepStarted(int64 Session, int32 Index)
{
	if (Session==AudioSession && Index>0 && PresentationData)
	{
		Queue(PresentationData->StartSound);
	}
}
void UQTEPresentationComponent::OnStepResolved(int64 Session, int32 Index, EAttackTimingResult Result)
{
	if (Session!=AudioSession || PlayedSteps.Contains(Index)) { return; }
	PlayedSteps.Add(Index); bResultPlayed=false;
	OnResolved(Session,Result);
}
void UQTEPresentationComponent::OnCancelled(int64 Session)
{
	if (Session == AudioSession) { ClearTimers(); AudioSession = 0; }
}
void UQTEPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearTimers();
	if (QTE.IsValid()) { QTE->Started.Remove(StartHandle); QTE->StepResolved.Remove(ResultHandle); QTE->StepStarted.Remove(StepStartHandle); QTE->Cancelled.Remove(CancelHandle); }
	Super::EndPlay(Reason);
}
