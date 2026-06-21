// Copyright draftDesk.

using UnrealBuildTool;

public class DraftDeskEditor : ModuleRules
{
	public DraftDeskEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"NavigationSystem",   // the live nav data we query (same nav the game uses)
			"ToolsetRegistry",    // UToolsetDefinition -> exposes AICallable tools over MCP
			"UnrealEd",           // GEditor / editor world
		});
	}
}
