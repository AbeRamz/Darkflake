// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Darkflake : ModuleRules
{
	public Darkflake(ReadOnlyTargetRules Target) : base(Target)
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
			"Darkflake",
			"Darkflake/Variant_Platforming",
			"Darkflake/Variant_Platforming/Animation",
			"Darkflake/Variant_Combat",
			"Darkflake/Variant_Combat/AI",
			"Darkflake/Variant_Combat/Animation",
			"Darkflake/Variant_Combat/Gameplay",
			"Darkflake/Variant_Combat/Interfaces",
			"Darkflake/Variant_Combat/UI",
			"Darkflake/Variant_SideScrolling",
			"Darkflake/Variant_SideScrolling/AI",
			"Darkflake/Variant_SideScrolling/Gameplay",
			"Darkflake/Variant_SideScrolling/Interfaces",
			"Darkflake/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
