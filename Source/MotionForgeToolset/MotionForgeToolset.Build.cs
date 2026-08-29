using UnrealBuildTool;

public class MotionForgeToolset : ModuleRules
{
	public MotionForgeToolset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ToolsetRegistry",  // UToolsetDefinition and UAgentSkill are public base classes here
				"MotionForge",      // the capability this adapter exposes, and the types in its signatures
			}
			);

		// Deliberately no dependency on ModelContextProtocol itself. Tools are registered with
		// ToolsetRegistry, and MCP picks them up from there - going direct would couple this module
		// to a transport it does not care about.
	}
}
