#include "UI/BattleResultPresentationComponent.h"
#include "UI/BattleResultPresentationData.h"
#include "UI/BattleResultWidget.h"
#include "UI/MagicPresentationComponent.h"
#include "UI/BattlePresentationComponent.h"
#include "Combat/ParryCounterProvider.h"
#include "UI/DefensePresentationComponent.h"
#include "Combat/BattleCoordinator.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"

UBattleResultPresentationComponent::UBattleResultPresentationComponent() { PrimaryComponentTick.bCanEverTick=false; }

void UBattleResultPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	Controller=Cast<APlayerController>(GetOwner());
	Coordinator=GetWorld()->GetSubsystem<UBattleCoordinator>();
	if (Coordinator.IsValid()) { StateHandle=Coordinator->OnBattleStateChanged().AddUObject(this,&ThisClass::HandleState); }
}

void UBattleResultPresentationComponent::HandleState(EBattleState Before,EBattleState After)
{
	if (After!=EBattleState::Victory && After!=EBattleState::Defeat) { return; }
	if (bActive || !Controller.IsValid() || !Controller->IsLocalController()) { return; }
	if (!PresentationData || !PresentationData->WidgetClass)
	{
		UE_LOG(LogBattleCoordinator,Error,TEXT("Battle result presentation data/widget is missing."));
		Coordinator->NotifyResultReady(); return;
	}
	bActive=true;
	if (APawn* Player=Controller->GetPawn())
	{
		TInlineComponentArray<UActorComponent*> Components(Player);
		for (UActorComponent* Component:Components)
		{
			if (Component->GetClass()->ImplementsInterface(UParryCounterProvider::StaticClass())) { IParryCounterProvider::Execute_ReleaseCounterImpact(Component); }
		}
		if (auto* Defense=Player->FindComponentByClass<UDefensePresentationComponent>()) { Defense->ReleaseImpactForBattleResult(); }
	}
	if (auto* Presentation=GetOwner()->FindComponentByClass<UBattlePresentationComponent>()) { Presentation->ReleaseParryFeedback(); }
	PreviousDilation=GetWorld()->GetWorldSettings()->TimeDilation;
	const float Factor=FMath::IsFinite(PresentationData->SlowFactor)?FMath::Clamp(PresentationData->SlowFactor,.01f,1.f):.15f;
	const float Duration=FMath::IsFinite(PresentationData->SlowDuration)?FMath::Clamp(PresentationData->SlowDuration,0.f,10.f):3.f;
	GetWorld()->GetWorldSettings()->SetTimeDilation(PreviousDilation*Factor);
	SlowEndsAt=FPlatformTime::Seconds()+Duration;
	SlowTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
	{
		if (!Coordinator.IsValid() || !Controller.IsValid()) { RestoreWorld(); return false; }
		if (FPlatformTime::Seconds()<SlowEndsAt) { return true; }
		SlowTicker.Reset(); ShowResult(); return false;
	}));
	UE_LOG(LogBattleCoordinator,Log,TEXT("Result slow motion started: Duration=%.2f Factor=%.2f"),Duration,Factor);
}

void UBattleResultPresentationComponent::ShowResult()
{
	Widget=CreateWidget<UBattleResultWidget>(Controller.Get(),PresentationData->WidgetClass);
	if (!Widget) { RestoreWorld(); Coordinator->NotifyResultReady(); return; }
	// Pause the world, rather than relying on a near-zero dilation. Slate remains interactive.
	bPausedByResult=Controller->SetPause(true);
	if (!bPausedByResult)
	{
		UE_LOG(LogBattleCoordinator,Error,TEXT("Result could not pause this world."));
		RestoreWorld(); Coordinator->NotifyResultReady(); return;
	}
	Coordinator->NotifyResultReady();
	Widget->AddToPlayerScreen(100);
	Widget->Present(Coordinator.Get());
	Controller->bShowMouseCursor=true;
	FInputModeUIOnly Input; Input.SetWidgetToFocus(Widget->TakeWidget());
	Controller->SetInputMode(Input);
	Widget->SetKeyboardFocus();
	UE_LOG(LogBattleCoordinator,Log,TEXT("BATTLE_RESULT_VISIBLE Paused=1 Victory=%d"),Coordinator->GetBattleResult().bVictory);
}

void UBattleResultPresentationComponent::RestoreWorld()
{
	if (SlowTicker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(SlowTicker); SlowTicker.Reset(); }
	if (bPausedByResult && Controller.IsValid()) { Controller->SetPause(false); }
	bPausedByResult=false;
	if (bActive && GetWorld()) { GetWorld()->GetWorldSettings()->SetTimeDilation(PreviousDilation); }
	bActive=false;
	if (Widget) { Widget->RemoveFromParent(); Widget=nullptr; }
	if (Controller.IsValid())
	{
		Controller->bShowMouseCursor=false;
		FInputModeGameOnly Input; Input.SetConsumeCaptureMouseDown(false); Controller->SetInputMode(Input);
	}
}

void UBattleResultPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	RestoreWorld();
	if (Coordinator.IsValid()) { Coordinator->OnBattleStateChanged().Remove(StateHandle); }
	Super::EndPlay(Reason);
}
