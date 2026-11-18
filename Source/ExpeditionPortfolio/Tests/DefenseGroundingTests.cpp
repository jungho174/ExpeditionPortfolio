#if WITH_DEV_AUTOMATION_TESTS
#include "UI/DefensePresentationComponent.h"
#include "UI/DefensePresentationData.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseGroundingTest,"ExpeditionPortfolio.Combat.DefensePresentation.GroundedDodge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseGroundingTest::RunTest(const FString& Parameters)
{
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);
	auto Box=[&](FVector Center,FVector Extent)
	{
		auto* Actor=World->SpawnActor<AActor>();auto* Shape=NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Shape);Shape->SetBoxExtent(Extent);
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Shape->SetCollisionObjectType(ECC_WorldStatic);
		Shape->SetCollisionResponseToAllChannels(ECR_Block);Shape->RegisterComponent();Actor->SetActorLocation(Center);return Actor;
	};
	Box(FVector(0,0,-10),FVector(1000,500,10));
	Box(FVector(-300,0,20),FVector(300,500,20));
	auto* Player=World->SpawnActor<ACharacter>();Player->GetCapsuleComponent()->SetCapsuleSize(30,90);
	auto* Mesh=Player->GetMesh();Mesh->SetRelativeLocation(FVector(0,0,-90));
	auto* Presentation=NewObject<UDefensePresentationComponent>(Player);Player->AddInstanceComponent(Presentation);Presentation->RegisterComponent();
	Presentation->Mesh=Mesh;Presentation->PresentationData=NewObject<UDefensePresentationData>(Presentation);
	auto Setup=[&](FVector Location,float Yaw)
	{
		Presentation->StopPresentation();Player->SetActorLocationAndRotation(Location,FRotator(0,Yaw,0));
		Presentation->OriginalMeshTransform=Mesh->GetRelativeTransform();
	};
	auto Begin=[&]()
	{
		Presentation->RetreatDuration=.14f;Presentation->ReturnStartTime=.68f;Presentation->ReturnDuration=.22f;
		Presentation->DodgeElapsed=0;Presentation->bDodging=true;Presentation->SetComponentTickEnabled(true);
	};
	Setup(FVector(100,0,92),0);
	const FTransform Original=Mesh->GetRelativeTransform(),ActorBefore=Player->GetActorTransform();
	TestTrue(TEXT("Unobstructed retreat keeps configured distance"),FMath::IsNearlyEqual(Presentation->PrepareDodgePath(),140.f));
	Begin();Presentation->UpdateDodge(.25f);
	TestTrue(TEXT("Dodge rises onto raised pavement"),FMath::IsNearlyEqual(Mesh->GetComponentLocation().Z,42.,.1));
	TestTrue(TEXT("Collision anchor stays fixed"),Player->GetActorTransform().Equals(ActorBefore));
	Presentation->StopPresentation();
	TestTrue(TEXT("Interrupt restores exact transform"),Mesh->GetRelativeTransform().Equals(Original));
	TestTrue(TEXT("Interrupt clears cached route"),Presentation->DodgeGroundPoints.IsEmpty());
	TestFalse(TEXT("Interrupt disables tick"),Presentation->IsComponentTickEnabled());
	Setup(FVector(-40,0,132),180);Presentation->PrepareDodgePath();Begin();Presentation->UpdateDodge(.25f);
	TestTrue(TEXT("Opposite direction descends to the lower road"),FMath::IsNearlyEqual(Mesh->GetComponentLocation().Z,2.,.1));
	Presentation->UpdateDodge(.7f);
	TestTrue(TEXT("Normal return restores exact home"),Mesh->GetRelativeTransform().Equals(Original));
	TestFalse(TEXT("Normal return disables tick"),Presentation->IsComponentTickEnabled());
	auto* Wall=Box(FVector(30,0,180),FVector(5,500,180));
	Setup(FVector(100,0,92),0);const float Limited=Presentation->PrepareDodgePath();
	TestTrue(TEXT("Wall shortens rather than cancels the safe portion"),Limited>0 && Limited<40);
	TestTrue(TEXT("Capsule remains in front of wall"),Presentation->DodgeGroundPoints.Last().X>=65);
	Begin();Presentation->UpdateDodge(.25f);
	TestTrue(TEXT("Limited movement still reaches hold phase"),Presentation->bDodging && Presentation->bHoldingPose);
	Presentation->StopPresentation();
	Wall->SetActorLocation(FVector(64,0,180));Setup(FVector(100,0,92),0);
	TestEqual(TEXT("No room gives a stationary dodge"),Presentation->PrepareDodgePath(),0.f);
	Begin();Presentation->UpdateDodge(.25f);
	TestTrue(TEXT("Stationary dodge remains active for the original timing"),Presentation->bDodging);
	TestTrue(TEXT("Stationary dodge does not enter wall"),FMath::IsNearlyEqual(Mesh->GetComponentLocation().X,100.));
	Presentation->UpdateDodge(.7f);
	TestTrue(TEXT("Stationary dodge also restores"),Mesh->GetRelativeTransform().Equals(Original));
	Wall->SetActorEnableCollision(false);
	Setup(FVector(970,300,92),180);
	const float CliffDistance=Presentation->PrepareDodgePath();
	TestTrue(TEXT("Missing ground truncates retreat before the cliff"),CliffDistance>=0 && CliffDistance<140);
	Setup(FVector(1500,0,92),180);
	TestEqual(TEXT("Unsupported starting point stays at its encounter anchor"),Presentation->PrepareDodgePath(),0.f);
	Begin();Presentation->UpdateDodge(.25f);
	TestTrue(TEXT("No-floor fallback never invents a position"),Mesh->GetComponentLocation().Equals(FVector(1500,0,2)));
	Presentation->StopPresentation();
	Setup(FVector(100,0,92),0);Presentation->PresentationData->DodgeGrounding.MaxStepHeight=10;
	TestTrue(TEXT("Excessive step truncates rather than crosses the route"),Presentation->PrepareDodgePath()<140);
	Presentation->PresentationData->DodgeDistance=0;
	TestEqual(TEXT("Authored zero-distance pose stays stationary"),Presentation->PrepareDodgePath(),0.f);
	TestEqual(TEXT("Zero-distance pose preserves exact root"),Presentation->DodgeGroundPoints.Last(),Player->GetActorLocation());
	World->DestroyWorld(false);return true;
}
#endif
