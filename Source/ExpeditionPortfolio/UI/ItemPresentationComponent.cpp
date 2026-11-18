#include "UI/ItemPresentationComponent.h"
#include "Combat/CombatItemData.h"
#include "Combat/CharacterCombatData.h"
#include "Combat/CombatantComponent.h"
#include "Combat/CombatantVisualData.h"
#include "Combat/BattleCoordinator.h"
#include "UI/CharacterItemUseData.h"
#include "UI/CombatSoundPlayback.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UItemPresentationComponent::UItemPresentationComponent() { PrimaryComponentTick.bCanEverTick=false; }
UCharacterItemUseData* UItemPresentationComponent::GetCharacterSettings() const
{
	const auto* Stats=GetOwner()->FindComponentByClass<UCombatantComponent>();
	return Stats && Stats->CharacterActions?Stats->CharacterActions->ItemUsePresentation.Get():nullptr;
}
float UItemPresentationComponent::GetExpectedDuration() const
{
	const auto* Settings=GetCharacterSettings();
	if (!Settings || !Settings->IsValidConfiguration()) { return 0; }
	return FMath::Max(Settings->Duration,Settings->Montage?Settings->Montage->GetPlayLength()/Settings->PlayRate:0);
}
bool UItemPresentationComponent::BeginUse(UCombatItemData* Item,int64 ActionId,AActor* Recipient)
{
	StopPresentation();
	const auto* Settings=GetCharacterSettings();
	if (!Item || !Item->IsValidConfiguration() || !ActionId || !IsValid(Recipient) || GetExpectedDuration()<=0) { return false; }
	ActiveId=ActionId;ActiveItem=Item;Target=Recipient;
	Mesh=GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (Settings->Montage)
	{
		if (!Mesh.IsValid() || !Mesh->GetAnimInstance() || Mesh->GetAnimInstance()->Montage_Play(Settings->Montage,Settings->PlayRate)<=0) { StopPresentation();return false; }
		ActiveMontage=Settings->Montage;
		FOnMontageEnded Ended;Ended.BindUObject(this,&ThisClass::OnMontageEnded,ActiveId);
		Mesh->GetAnimInstance()->Montage_SetEndDelegate(Ended,ActiveMontage);
		FOnMontageBlendingOutStarted BlendingOut;BlendingOut.BindUObject(this,&ThisClass::OnMontageEnded,ActiveId);
		Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(BlendingOut,ActiveMontage);
	}
	if (Item->Effect)
	{
		const auto* RecipientMesh=Recipient->FindComponentByClass<USkeletalMeshComponent>();
		const FVector Location=RecipientMesh?RecipientMesh->GetComponentLocation():Recipient->GetActorLocation();
		Effect=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),Item->Effect,Location,Recipient->GetActorRotation());
	}
	GetWorld()->GetTimerManager().SetTimer(ApplyTimer,FTimerDelegate::CreateUObject(this,&ThisClass::Apply,ActiveId),Settings->ApplyTime,false);
	GetWorld()->GetTimerManager().SetTimer(FinishTimer,FTimerDelegate::CreateUObject(this,&ThisClass::Finish,false),GetExpectedDuration(),false);
	return true;
}
void UItemPresentationComponent::Apply(int64 ExpectedId)
{
	if (!ActiveId || ActiveId!=ExpectedId || !ActiveItem) { return; }
	if (GetWorld()->GetSubsystem<UBattleCoordinator>()->NotifyPlayerActionHit(ExpectedId))
	{
		const auto& Sound=ActiveItem->UseSound;
		CombatSoundPlayback::Play(GetOwner(),Sound.Sound,Target.IsValid()?Target->GetActorLocation():GetOwner()->GetActorLocation(),Sound.Volume,Sound.Delay,Sound.StartTime);
	}
}
void UItemPresentationComponent::OnMontageEnded(UAnimMontage* Montage,bool bInterrupted,int64 ExpectedId)
{ if (bInterrupted && ActiveId==ExpectedId && Montage==ActiveMontage) { Finish(true); } }
void UItemPresentationComponent::Finish(bool bInterrupted)
{
	const int64 Id=ActiveId;StopPresentation();
	if (Id && GetWorld()) { GetWorld()->GetSubsystem<UBattleCoordinator>()->NotifyPlayerActionFinished(Id,bInterrupted); }
}
void UItemPresentationComponent::StopPresentation()
{
	ActiveId=0;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ApplyTimer);GetWorld()->GetTimerManager().ClearTimer(FinishTimer); }
	if (Mesh.IsValid() && Mesh->GetAnimInstance() && ActiveMontage)
	{
		FOnMontageEnded Empty;Mesh->GetAnimInstance()->Montage_SetEndDelegate(Empty,ActiveMontage);
		FOnMontageBlendingOutStarted EmptyBlendingOut;Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(EmptyBlendingOut,ActiveMontage);
		Mesh->GetAnimInstance()->Montage_Stop(.1f,ActiveMontage);
	}
	if (IsValid(Effect)) { Effect->DestroyComponent(); } Effect=nullptr;
	ActiveItem=nullptr;ActiveMontage=nullptr;Mesh.Reset();Target.Reset();
}
void UItemPresentationComponent::EndPlay(const EEndPlayReason::Type Reason) { StopPresentation();Super::EndPlay(Reason); }
