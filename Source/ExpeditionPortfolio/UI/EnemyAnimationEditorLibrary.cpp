#include "UI/EnemyAnimationEditorLibrary.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/AnimNotify_EnemyHit.h"
#endif

bool UEnemyAnimationEditorLibrary::BuildIdleGraph(UAnimBlueprint* Blueprint,UAnimSequence* Idle)
{
#if WITH_EDITOR
	if (!Blueprint || !Idle) { return false; }
	UEdGraph* Graph=nullptr;
	for (UEdGraph* Candidate:Blueprint->FunctionGraphs) { if (Candidate && Candidate->GetFName()==TEXT("AnimGraph")) { Graph=Candidate; break; } }
	if (!Graph) { return false; }
	UAnimGraphNode_Root* Root=nullptr;
	for (UEdGraphNode* Node:Graph->Nodes) { if (auto* Candidate=Cast<UAnimGraphNode_Root>(Node)) { Root=Candidate; break; } }
	if (!Root) { return false; }
	// This helper only authors newly created project-owned graphs.
	if (Graph->Nodes.Num()>1) { return true; }
	auto* Player=NewObject<UAnimGraphNode_SequencePlayer>(Graph); Player->CreateNewGuid(); Graph->AddNode(Player); Player->PostPlacedNewNode(); Player->AllocateDefaultPins();
	Player->Node.SetSequence(Idle); Player->Node.SetLoopAnimation(true); Player->NodePosX=-400;
	auto* SlotNode=NewObject<UAnimGraphNode_Slot>(Graph); SlotNode->CreateNewGuid(); Graph->AddNode(SlotNode); SlotNode->PostPlacedNewNode(); SlotNode->AllocateDefaultPins();
	SlotNode->Node.SlotName=TEXT("DefaultSlot"); SlotNode->NodePosX=-200;
	UEdGraphPin* Output=nullptr; UEdGraphPin* Source=nullptr; UEdGraphPin* SlotOutput=nullptr; UEdGraphPin* RootInput=nullptr;
	for (auto* Pin:Player->Pins) { if (Pin->Direction==EGPD_Output) { Output=Pin; break; } }
	for (auto* Pin:SlotNode->Pins) { if (Pin->Direction==EGPD_Output) { SlotOutput=Pin; } else if (Pin->PinName==TEXT("Source")) { Source=Pin; } }
	for (auto* Pin:Root->Pins) { if (Pin->Direction==EGPD_Input && Pin->PinName==TEXT("Result")) { RootInput=Pin; break; } }
	if (!Output || !Source || !SlotOutput || !RootInput) { return false; }
	Output->MakeLinkTo(Source); SlotOutput->MakeLinkTo(RootInput);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Blueprint->Status!=BS_Error;
#else
	return false;
#endif
}

bool UEnemyAnimationEditorLibrary::BuildComboMontage(UAnimMontage* Montage,const TArray<UAnimSequence*>& Clips,const TArray<float>& Rates,const TArray<float>& HitTimes,UAnimSequence* LeadPose)
{
#if WITH_EDITOR
	if (!Montage || Clips.IsEmpty() || Rates.Num()!=Clips.Num() || HitTimes.Num()!=Clips.Num()) { return false; }
	Montage->Modify(); Montage->SlotAnimTracks.Empty(); Montage->Notifies.Empty();
	FSlotAnimationTrack& Track=Montage->SlotAnimTracks.AddDefaulted_GetRef(); Track.SlotName=TEXT("DefaultSlot");
	float Cursor=0;
	if (LeadPose)
	{
		FAnimSegment& Lead=Track.AnimTrack.AnimSegments.AddDefaulted_GetRef(); Lead.SetAnimReference(LeadPose);
		Lead.AnimStartTime=0; Lead.AnimEndTime=.5f; Lead.AnimPlayRate=1; Lead.LoopingCount=1; Lead.StartPos=0; Cursor=.5f;
	}
	TArray<float> MarkerTimes;
	for (int32 Index=0;Index<Clips.Num();++Index)
	{
		if (!Clips[Index] || Rates[Index]<=0 || HitTimes[Index]<0 || HitTimes[Index]>=Clips[Index]->GetPlayLength()) { return false; }
		FAnimSegment& Segment=Track.AnimTrack.AnimSegments.AddDefaulted_GetRef(); Segment.SetAnimReference(Clips[Index]);
		Segment.StartPos=Cursor; Segment.AnimStartTime=0; Segment.AnimEndTime=Clips[Index]->GetPlayLength(); Segment.AnimPlayRate=Rates[Index]; Segment.LoopingCount=1;
		MarkerTimes.Add(Cursor+HitTimes[Index]/Rates[Index]);
		Cursor+=Segment.GetLength();
	}
	Montage->SetCompositeLength(Cursor); Montage->BlendIn.SetBlendTime(.1f); Montage->BlendOut.SetBlendTime(.15f);
	Montage->PostEditChange();
	// Link markers only after the final track length is established. PostEditChange can
	// recalculate segments; linking while the track is still growing moves later markers.
	for (int32 Index=0;Index<MarkerTimes.Num();++Index)
	{
		FAnimNotifyEvent& Event=Montage->Notifies.AddDefaulted_GetRef(); auto* Notify=NewObject<UAnimNotify_EnemyHit>(Montage); Notify->HitIndex=Index;
		Event.Notify=Notify; Event.NotifyName=TEXT("EnemyHit"); Event.MontageTickType=EMontageNotifyTickType::BranchingPoint;
		Event.Link(Montage,MarkerTimes[Index]);
	}
	Montage->PostEditChange();
	for (int32 Index=0;Index<MarkerTimes.Num();++Index)
	{ if (!FMath::IsNearlyEqual(Montage->Notifies[Index].GetTriggerTime(),MarkerTimes[Index],.002f)) { return false; } }
	return true;
#else
	return false;
#endif
}
