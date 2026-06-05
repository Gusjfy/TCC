// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TCC : ModuleRules
{
	public TCC(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "UMG"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "SQLiteCore",
            "SQLiteSupport"
        });
    }
}
