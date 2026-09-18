// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AI_Gavin_Project : ModuleRules
{
	public AI_Gavin_Project(ReadOnlyTargetRules Target) : base(Target)
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
			"AI_Gavin_Project",
			"AI_Gavin_Project/Variant_Platforming",
			"AI_Gavin_Project/Variant_Platforming/Animation",
			"AI_Gavin_Project/Variant_Combat",
			"AI_Gavin_Project/Variant_Combat/AI",
			"AI_Gavin_Project/Variant_Combat/Animation",
			"AI_Gavin_Project/Variant_Combat/Gameplay",
			"AI_Gavin_Project/Variant_Combat/Interfaces",
			"AI_Gavin_Project/Variant_Combat/UI",
			"AI_Gavin_Project/Variant_SideScrolling",
			"AI_Gavin_Project/Variant_SideScrolling/AI",
			"AI_Gavin_Project/Variant_SideScrolling/Gameplay",
			"AI_Gavin_Project/Variant_SideScrolling/Interfaces",
			"AI_Gavin_Project/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
