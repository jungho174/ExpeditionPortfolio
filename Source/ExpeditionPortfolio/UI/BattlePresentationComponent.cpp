#include "UI/BattlePresentationComponent.h"
#include "Combat/CombatantVisualData.h"
#include "Combat/CharacterActionData.h"
#include "UI/ActionCameraData.h"
#include "UI/BattleCameraRig.h"
#include "UI/ParryFeedbackData.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/PlatformTime.h"
#include "Combat/BattleCoordinator.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UI/CombatSoundPlayback.h"

UBattlePresentationComponent::UBattlePresentationComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UBattlePresentationComponent::PlayUIAudio(EBattleUIAudio Cue)
{
	if (!UIAudioData || !Controller.IsValid() || !Controller->IsLocalController()) { return; }
	const auto& Sound=UIAudioData->Get(Cue);
	CombatSoundPlayback::Play(GetOwner(),Sound.Sound,FVector::ZeroVector,Sound.Volume,Sound.Delay,Sound.StartTime,true);
}
float UBattlePresentationComponent::GetTargetConfirmationDuration() const
{
	if (!PresentationData || !bPresenting) { return 0.47f; }
	const float Blend = FMath::IsFinite(PresentationData->BlendDuration) ? FMath::Clamp(PresentationData->BlendDuration, 0.f, 3.f) : 0.f;
	const float Hold = FMath::IsFinite(PresentationData->TargetConfirmHoldDuration) ? FMath::Clamp(PresentationData->TargetConfirmHoldDuration, 0.f, 2.f) : 0.12f;
	return FMath::Max(0.01f, Blend + Hold);
}
void UBattlePresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	Controller = Cast<APlayerController>(GetOwner());
	Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>();
	if (Coordinator.IsValid())
	{
  DefenseHandle=Coordinator->OnDefenseResultChanged().AddWeakLambda(this,[this](EBattleDefenseResult Result)
  {
   if (Result==EBattleDefenseResult::ParrySuccess && Coordinator.IsValid() && !Coordinator->IsParryCounterActive()) { BeginParryFeedback(0,false); }
  });
  StateHandle = Coordinator->OnBattleStateChanged().AddUObject(this, &ThisClass::HandleState);
		PhaseHandle = Coordinator->OnPlayerPhaseChanged().AddUObject(this, &ThisClass::HandlePhase);
		TargetHandle = Coordinator->OnTargetSelectionChanged().AddUObject(this, &ThisClass::HandleTargetSelection);
		TargetMoveHandle=Coordinator->OnTargetNavigation().AddWeakLambda(this,[this]()
		{
			if (Coordinator.IsValid() && Coordinator->IsSelectingAlly()) { PlayUIAudio(EBattleUIAudio::AllyTargetMove); }
		});
		TargetConfirmHandle=Coordinator->OnTargetConfirmed().AddWeakLambda(this,[this]()
		{
			if (!Coordinator.IsValid()) { return; }
			if (Coordinator->IsConfirmedAlly()) { PlayUIAudio(EBattleUIAudio::AllyTargetMove); }
			else if (PresentationData)
			{
				CombatSoundPlayback::Play(GetOwner(),PresentationData->TargetSelectionSound,FVector::ZeroVector,
					PresentationData->TargetSelectionSoundVolume,PresentationData->TargetSelectionSoundDelay,PresentationData->TargetSelectionSoundStartTime,true);
			}
		});
	}
}
float UBattlePresentationComponent::BeginBattleIntro(UBattleCoordinator* Battle, int64 BattleId)
{
	if (!PresentationData || !PresentationData->CameraRigClass || !Controller.IsValid() || !Controller->IsLocalController()
		|| !FMath::IsFinite(PresentationData->IntroDuration) || PresentationData->IntroDuration < 0.05f || PresentationData->IntroDuration > 5) { return 0; }
	Coordinator = Battle;
	ExplorationViewTarget = Controller->GetViewTarget();
	ExplorationRotation = Controller->GetControlRotation();
	for (int32 I = Rigs.Num(); I < 2; ++I)
	{
		auto* Rig = GetWorld()->SpawnActor<ABattleCameraRig>(PresentationData->CameraRigClass);
		if (!Rig) { RestoreExplorationCamera(); return 0; }
		Rigs.Add(Rig);
	}
	bPresenting = true;
	ShowShot(PresentationData->CommandShot, TEXT("Command"));
	GetWorld()->GetTimerManager().SetTimer(IntroTimer, FTimerDelegate::CreateWeakLambda(this, [this, BattleId]()
	{
		if (Coordinator.IsValid()) { Coordinator->NotifyBattleIntroFinished(BattleId); }
	}), PresentationData->IntroDuration, false);
	return PresentationData->IntroDuration;
}
void UBattlePresentationComponent::ShowShot(const FBattleCameraShot& Shot, FName Name)
{
	if (!bPresenting || !Coordinator.IsValid() || !Controller.IsValid() || Rigs.Num() != 2) { return; }
	AActor* Target = Coordinator->IsSelectingTarget() ? Coordinator->GetSelectedTarget() : Coordinator->GetActiveEnemyActor();
	if (!Target || (CurrentShot == Name && CameraTarget.Get() == Target && CameraSubject.Get()==Coordinator->GetActivePlayerPawn())) { return; }
	CameraTarget = Target;
	CameraSubject=Coordinator->GetActivePlayerPawn();
	RigIndex = 1 - RigIndex;
	if (Coordinator->IsSelectingAllEnemies())
	{
		Rigs[RigIndex]->ConfigureGroupShot(Shot,Coordinator->GetActivePlayerPawn(),Coordinator->GetSelectedTargets());
	}
	else if (Name==TEXT("Attack") && Coordinator->IsActiveActionAreaAttack())
	{
		Rigs[RigIndex]->ConfigureGroupShot(Shot,Coordinator->GetActivePlayerPawn(),Coordinator->GetActiveActionTargets());
	}
	else { Rigs[RigIndex]->ConfigureShot(Shot, Coordinator->GetActivePlayerPawn(), Target); }
	const float Blend = FMath::IsFinite(PresentationData->BlendDuration) ? FMath::Clamp(PresentationData->BlendDuration, 0.0f, 3.0f) : 0;
	Controller->SetViewTargetWithBlend(Rigs[RigIndex], Blend, VTBlend_Cubic, 0, true);
	CurrentShot = Name;
}
void UBattlePresentationComponent::HandleState(EBattleState Before, EBattleState After)
{
	if (Before!=After && After==EBattleState::PlayerCommand) { PlayUIAudio(EBattleUIAudio::AllyTurn); }
	if (Before!=After && After==EBattleState::EnemyTelegraph) { PlayUIAudio(EBattleUIAudio::EnemyTurn); }
 if (After==EBattleState::ReturnToField || After==EBattleState::Exploration) { ReleaseParryFeedback(); }
 if (After != EBattleState::PlayerCommand) { LastSelectionSoundTarget.Reset(); }
	if (!PresentationData) { return; }
	if (After == EBattleState::PlayerCommand) { ShowShot(PresentationData->CommandShot, TEXT("Command")); }
	else if (After == EBattleState::EnemyTelegraph) { ShowShot(PresentationData->EnemyShot, TEXT("Enemy")); }
	else if (After == EBattleState::ReturnToField || After == EBattleState::Exploration) { RestoreExplorationCamera(); }
}
void UBattlePresentationComponent::HandlePhase(EPlayerActionPhase Phase)
{
	if (!PresentationData) { return; }
	const auto* Action=Coordinator.IsValid()?Coordinator->GetActiveCharacterAction():nullptr;
	if (Phase == EPlayerActionPhase::Preparing)
	{ ShowShot(UActionCameraData::ResolveShot(Action,true,PresentationData->PreparationShot),TEXT("Preparation")); }
	else if (Phase == EPlayerActionPhase::Executing)
	{ ShowShot(UActionCameraData::ResolveShot(Action,false,PresentationData->AttackShot),TEXT("Attack")); }
}
void UBattlePresentationComponent::PresentCounterAction(UCharacterActionData* Action)
{
	if (!PresentationData) { return; }
	ShowShot(UActionCameraData::ResolveShot(Action,false,PresentationData->EnemyShot),TEXT("Counter"));
}
void UBattlePresentationComponent::HandleTargetSelection()
{
	AActor* Selected = Coordinator.IsValid() && Coordinator->IsSelectingTarget() ? Coordinator->GetSelectedTarget() : nullptr;
	const bool bNewTarget = Selected && Selected != LastSelectionSoundTarget.Get();
	const bool bEntering=Selected && !LastSelectionSoundTarget.IsValid();
	LastSelectionSoundTarget = Selected;
	if (!PresentationData || !Coordinator.IsValid() || Coordinator->GetCurrentState() != EBattleState::PlayerCommand) { return; }
	if (Coordinator->IsSelectingAlly() && bEntering) { PlayUIAudio(EBattleUIAudio::AllyTargetStart); }
	if (bNewTarget && !Coordinator->IsSelectingAlly() && Controller.IsValid() && Controller->IsLocalController())
	{
		CombatSoundPlayback::Play(GetOwner(), PresentationData->TargetSelectionSound, FVector::ZeroVector,
			PresentationData->TargetSelectionSoundVolume, PresentationData->TargetSelectionSoundDelay,
			PresentationData->TargetSelectionSoundStartTime, true);
	}
	if (Coordinator->IsSelectingTarget())
	{
		const auto& Shot=Coordinator->IsSelectingAlly()?PresentationData->AllyTargetSelectionShot:UActionCameraData::ResolveTargetShot(Coordinator->GetTargetSelectionAction(),PresentationData->TargetSelectionShot);
		ShowShot(Shot,Coordinator->IsSelectingAllEnemies()?TEXT("AllTargetSelection"):TEXT("TargetSelection"));
	}
	else { ShowShot(PresentationData->CommandShot, TEXT("Command")); }
}
void UBattlePresentationComponent::RestoreExplorationCamera()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(IntroTimer); }
	if (bPresenting && Controller.IsValid())
	{
		Controller->SetControlRotation(ExplorationRotation);
		AActor* Target = ExplorationViewTarget.IsValid() ? ExplorationViewTarget.Get() : Controller->GetPawn();
		if (Target) { Controller->SetViewTargetWithBlend(Target, 0); }
	}
	bPresenting = false;
	CurrentShot = NAME_None;
	CameraTarget.Reset();
	CameraSubject.Reset();
	LastSelectionSoundTarget.Reset();
	ExplorationViewTarget.Reset();
	for (ABattleCameraRig* Rig : Rigs) { if (IsValid(Rig)) { Rig->Destroy(); } }
	Rigs.Empty();
}
void UBattlePresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	RestoreExplorationCamera();
	if (Coordinator.IsValid())
	{
		Coordinator->OnBattleStateChanged().Remove(StateHandle);
		Coordinator->OnPlayerPhaseChanged().Remove(PhaseHandle);
		Coordinator->OnTargetSelectionChanged().Remove(TargetHandle);
		Coordinator->OnTargetNavigation().Remove(TargetMoveHandle);
		Coordinator->OnTargetConfirmed().Remove(TargetConfirmHandle);
	}
	ReleaseParryFeedback();
	if (Coordinator.IsValid()) { Coordinator->OnDefenseResultChanged().Remove(DefenseHandle); }
	Super::EndPlay(Reason);
}

bool UBattlePresentationComponent::BeginParryFeedback(int64 ActionId, bool bComplete)
{
 ReleaseParryFeedback();
 if (!ParryFeedback || !Coordinator.IsValid() || !Controller.IsValid()) { return false; }
 AActor* Player=Coordinator->GetActivePlayerPawn();
 if (!IsValid(Player)) { return false; }
 const auto& Style=bComplete?ParryFeedback->Complete:ParryFeedback->Intermediate;
 const FVector Location=UCombatantVisualData::ResolveAnchor(Player,bComplete?ECombatantAnchor::ParryComplete:ECombatantAnchor::ParryIntermediate);
 CombatSoundPlayback::Play(Player,Style.Sound.Sound,Location,Style.Sound.Volume,Style.Sound.Delay,Style.Sound.StartTime);
 const float Duration=FMath::IsFinite(Style.SlowDuration)?FMath::Clamp(Style.SlowDuration,0.f,2.f):0;
 const float Factor=FMath::IsFinite(Style.SlowFactor)?FMath::Clamp(Style.SlowFactor,.01f,1.f):1;
 if (Style.Effect)
 {
  ParryFX=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),Style.Effect,Location,Player->GetActorRotation(),Style.EffectScale);
  if (ParryFX && Duration>0) { ParryFX->SetForceSolo(true); ParryFX->SetCustomTimeDilation(1.f/Factor); }
 }
 if (Controller->PlayerCameraManager && Style.CameraShake) { ParryShake=Controller->PlayerCameraManager->StartCameraShake(Style.CameraShake,Style.CameraShakeScale); }
 UE_LOG(LogBattleCoordinator,Log,TEXT("Shared parry feedback: Complete=%d Duration=%.3f Sound=%s"),bComplete,Duration,*GetNameSafe(Style.Sound.Sound));
 if (Duration<=0) { return false; }
 SlowWorld=GetWorld()->GetWorldSettings(); PreviousParryDilation=SlowWorld->TimeDilation;
 SlowWorld->SetTimeDilation(PreviousParryDilation*Factor);
 const double EndsAt=FPlatformTime::Seconds()+Duration;
 ParryTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,EndsAt,ActionId,bComplete](float)
 {
  if (FPlatformTime::Seconds()<EndsAt) { return true; }
  ParryTicker.Reset(); ReleaseParryFeedback();
  if (bComplete && Coordinator.IsValid()) { Coordinator->NotifyCounterFeedbackFinished(ActionId); }
  return false;
 }));
 return bComplete;
}
void UBattlePresentationComponent::ReleaseParryFeedback()
{
 if (ParryTicker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(ParryTicker); ParryTicker.Reset(); }
 if (SlowWorld.IsValid()) { SlowWorld->SetTimeDilation(PreviousParryDilation); SlowWorld.Reset(); }
 if (IsValid(ParryFX)) { ParryFX->DestroyComponent(); } ParryFX=nullptr;
 if (Controller.IsValid() && Controller->PlayerCameraManager && ParryShake) { Controller->PlayerCameraManager->StopCameraShake(ParryShake,true); }
 ParryShake=nullptr;
}
