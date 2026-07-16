// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ExpeditionPortfolio : ModuleRules
{
	public ExpeditionPortfolio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ExpeditionPortfolio",
			"ExpeditionPortfolio/Variant_Platforming",
			"ExpeditionPortfolio/Variant_Platforming/Animation",
			"ExpeditionPortfolio/Variant_Combat",
			"ExpeditionPortfolio/Variant_Combat/AI",
			"ExpeditionPortfolio/Variant_Combat/Animation",
			"ExpeditionPortfolio/Variant_Combat/Gameplay",
			"ExpeditionPortfolio/Variant_Combat/Interfaces",
			"ExpeditionPortfolio/Variant_Combat/UI",
			"ExpeditionPortfolio/Variant_SideScrolling",
			"ExpeditionPortfolio/Variant_SideScrolling/AI",
			"ExpeditionPortfolio/Variant_SideScrolling/Gameplay",
			"ExpeditionPortfolio/Variant_SideScrolling/Interfaces",
			"ExpeditionPortfolio/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
