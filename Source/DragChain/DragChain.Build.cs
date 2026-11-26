// Copyright (c) 2026 ysysyxg (https://github.com/ysysyxg)
// SPDX-License-Identifier: MIT

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
