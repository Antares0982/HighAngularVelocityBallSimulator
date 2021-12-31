// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class test27cpp : ModuleRules
{
	public test27cpp(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay" });
	}
}
