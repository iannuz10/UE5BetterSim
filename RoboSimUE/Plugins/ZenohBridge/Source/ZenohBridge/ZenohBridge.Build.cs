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
            // 1. Link the Lib
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "zenohc.lib"));

            // 2. Delay Load the DLL
            PublicDelayLoadDLLs.Add("zenohc.dll");

            // 3. AUTOMATIC COPY FIX (Corrected)
            // Source: Plugins/ZenohBridge/Source/ThirdParty/bin/zenohc.dll
            string DllSource = Path.Combine(BinPath, "zenohc.dll");
            
            // Destination 1: Project Binaries (Where the .exe lives)
            // We use ModuleDirectory to go up: Plugins/ZenohBridge/Source/ZenohBridge -> ProjectRoot/Binaries/Win64
            string ProjectBinaries = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "..", "Binaries", "Win64"));
            string DllDest = Path.Combine(ProjectBinaries, "zenohc.dll");

            // Destination 2: Plugin Binaries (Where the Editor looks for plugin DLLs)
            string PluginBinaries = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "Binaries", "Win64"));
            string PluginDllDest = Path.Combine(PluginBinaries, "zenohc.dll");

            // Tell Unreal to copy it to BOTH locations
            RuntimeDependencies.Add(DllDest, DllSource);
            RuntimeDependencies.Add(PluginDllDest, DllSource);
        }
	}
}