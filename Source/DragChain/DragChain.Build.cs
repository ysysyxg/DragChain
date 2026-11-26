// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DragChain : ModuleRules
{
	public DragChain(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"MeshDescription",
			"StaticMeshDescription",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
