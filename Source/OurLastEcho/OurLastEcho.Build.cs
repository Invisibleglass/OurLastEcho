// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OurLastEcho : ModuleRules
{
	public OurLastEcho(ReadOnlyTargetRules Target) : base(Target)
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

		PrivateDependencyModuleNames.AddRange(new string[] { "Landscape" });

		// Editor-only builder helpers (EchoEditorLibrary)
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.AddRange(new string[] {
			"OurLastEcho",
			"OurLastEcho/Echo",
			"OurLastEcho/Variant_Platforming",
			"OurLastEcho/Variant_Platforming/Animation",
			"OurLastEcho/Variant_Combat",
			"OurLastEcho/Variant_Combat/AI",
			"OurLastEcho/Variant_Combat/Animation",
			"OurLastEcho/Variant_Combat/Gameplay",
			"OurLastEcho/Variant_Combat/Interfaces",
			"OurLastEcho/Variant_Combat/UI",
			"OurLastEcho/Variant_SideScrolling",
			"OurLastEcho/Variant_SideScrolling/AI",
			"OurLastEcho/Variant_SideScrolling/Gameplay",
			"OurLastEcho/Variant_SideScrolling/Interfaces",
			"OurLastEcho/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
