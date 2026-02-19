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
				"Projects",
				"Json",
				"JsonUtilities"
			}
		);

		string PluginRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string ThirdPartyPath = Path.Combine(PluginRoot, "Source", "ThirdParty");
		string IncludePath = Path.Combine(ThirdPartyPath, "include");
		
		// Add Headers
		PublicIncludePaths.Add(IncludePath);
		
		string PlatformFolder = "";
		string LibFileName = "";
		string BinaryFileName = "";
		string SourceBinPath = "";

		if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PlatformFolder = "Win64";
            LibFileName = "zenohc.lib";
            BinaryFileName = "zenohc.dll";
            // On Windows, DLLs are in 'bin'
            SourceBinPath = Path.Combine(ThirdPartyPath, "bin", PlatformFolder, BinaryFileName);
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PlatformFolder = "Mac";
            LibFileName = "libzenohc.dylib";
            BinaryFileName = "libzenohc.dylib"; 
            SourceBinPath = Path.Combine(ThirdPartyPath, "lib", PlatformFolder, BinaryFileName);
        }

        // 4. Link and Copy 
        if (!string.IsNullOrEmpty(PlatformFolder))
        {
            // A. Link the Lib (Compile time)
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "lib", PlatformFolder, LibFileName));

            // B. Delay Load (Runtime)
            PublicDelayLoadDLLs.Add(BinaryFileName);

            // C. MANUAL COPY
            
            // Destination 1: Project Binaries 
            string ProjectBinaries = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "..", "Binaries", PlatformFolder));
            string ProjectDest = Path.Combine(ProjectBinaries, BinaryFileName);

            // Destination 2: Plugin Binaries
            string PluginBinaries = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "Binaries", PlatformFolder));
            string PluginDest = Path.Combine(PluginBinaries, BinaryFileName);

            // Execute Copy
            RuntimeDependencies.Add(ProjectDest, SourceBinPath);
            RuntimeDependencies.Add(PluginDest, SourceBinPath);
        }
	}
}