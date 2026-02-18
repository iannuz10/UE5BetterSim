using UnrealBuildTool;
using System.IO;

public class ZenohBridge : ModuleRules
{
	public ZenohBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		// Keep unity disabled for safety
		bUseUnity = false;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects"
			}
		);

		string PluginRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string ThirdPartyPath = Path.Combine(PluginRoot, "Source", "ThirdParty");
		string LibPath = Path.Combine(ThirdPartyPath, "lib");
		string BinPath = Path.Combine(ThirdPartyPath, "bin");

		// 1. Include Headers
		PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// 2. Link the .lib (Import Library)
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "zenohc.lib"));

			// 3. DELAY LOAD the .dll
			// This stops the engine from crashing if the DLL is missing at startup
			PublicDelayLoadDLLs.Add("zenohc.dll");

			// 4. Ensure the DLL is copied to the Binary folder
			RuntimeDependencies.Add("$(BinaryOutputDir)/zenohc.dll", Path.Combine(BinPath, "zenohc.dll"));
		}
	}
}