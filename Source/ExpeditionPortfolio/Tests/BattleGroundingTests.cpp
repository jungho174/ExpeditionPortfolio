#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/BattleGrounding.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleGroundPathTest,"ExpeditionPortfolio.Combat.Movement.GroundedPaths",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBattleGroundPathTest::RunTest(const FString& Parameters)
{
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);
	auto Box=[&](FVector Center,FVector Extent)
	{
		auto* Actor=World->SpawnActor<AActor>();auto* Shape=NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Shape);Shape->SetBoxExtent(Extent);
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Shape->SetCollisionObjectType(ECC_WorldStatic);
		Shape->SetCollisionResponseToAllChannels(ECR_Block);Shape->RegisterComponent();Actor->SetActorLocation(Center);return Actor;
	};
	Box(FVector(0,0,-10),FVector(2000,1000,10));
	Box(FVector(-300,0,20),FVector(300,1000,20));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GroundPathTest),true);
	FBattleGroundingSettings Settings;const FVector Capsule(30,30,90);
	TArray<FVector> Points;FString Failure;
	TestTrue(TEXT("Downhill step path exists"),UBattleGroundingLibrary::BuildPath(World,FVector(-400,0,132),FVector(400,0,132),Capsule,Settings,Query,Points,Failure));
	if (!Points.IsEmpty())
	{
		TestTrue(TEXT("Initial support is the raised pavement"),FMath::IsNearlyEqual(Points[0].Z,132.f,1.f));
		TestTrue(TEXT("Approach ends on the lower road"),FMath::IsNearlyEqual(Points.Last().Z,92.f,1.f));
		const FVector Middle=UBattleGroundingLibrary::EvaluatePath(Points,.5f);
		TestTrue(TEXT("Height follows the step rather than the initial Z"),Middle.Z>=92 && Middle.Z<=132.1);
		TestEqual(TEXT("Return reaches the same supported home"),UBattleGroundingLibrary::EvaluatePath(Points,0),Points[0]);
	}
	TestTrue(TEXT("Uphill return also clears the step"),UBattleGroundingLibrary::BuildPath(World,FVector(400,0,92),FVector(-400,0,92),Capsule,Settings,Query,Points,Failure));
	if (!Points.IsEmpty()) { TestTrue(TEXT("Uphill ends above the step"),FMath::IsNearlyEqual(Points.Last().Z,132.f,1.f)); }
	TestFalse(TEXT("Missing ground cancels the entire path"),UBattleGroundingLibrary::BuildPath(World,FVector(400,0,92),FVector(2300,0,92),Capsule,Settings,Query,Points,Failure));
	TestTrue(TEXT("Failed paths leave no partial route"),Points.IsEmpty());
	Box(FVector(200,0,150),FVector(20,500,150));
	TestFalse(TEXT("Cannot walk through a solid wall"),UBattleGroundingLibrary::BuildPath(World,FVector(-400,0,132),FVector(400,0,132),Capsule,Settings,Query,Points,Failure));
	Settings.MaxStepHeight=10;
	TestFalse(TEXT("Step higher than the configured limit is rejected"),UBattleGroundingLibrary::BuildPath(World,FVector(400,700,92),FVector(-400,700,92),Capsule,Settings,Query,Points,Failure));
	Settings.bEnabled=false;
	TestTrue(TEXT("Explicit cinematic opt-out keeps the supplied path"),UBattleGroundingLibrary::BuildPath(World,FVector(-400,0,132),FVector(400,0,132),Capsule,Settings,Query,Points,Failure));
	TestEqual(TEXT("Opt-out preserves authored height"),Points.Last().Z,132.);
	World->DestroyWorld(false);return true;
}
#endif
