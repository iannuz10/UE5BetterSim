// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RoboSimUE : ModuleRules
{
	public RoboSimUE(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"RoboSimUE",
			"RoboSimUE/Variant_Platforming",
			"RoboSimUE/Variant_Platforming/Animation",
			"RoboSimUE/Variant_Combat",
			"RoboSimUE/Variant_Combat/AI",
			"RoboSimUE/Variant_Combat/Animation",
			"RoboSimUE/Variant_Combat/Gameplay",
			"RoboSimUE/Variant_Combat/Interfaces",
			"RoboSimUE/Variant_Combat/UI",
			"RoboSimUE/Variant_SideScrolling",
			"RoboSimUE/Variant_SideScrolling/AI",
			"RoboSimUE/Variant_SideScrolling/Gameplay",
			"RoboSimUE/Variant_SideScrolling/Interfaces",
			"RoboSimUE/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
