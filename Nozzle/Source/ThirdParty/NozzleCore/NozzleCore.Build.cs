using System.IO;
using UnrealBuildTool;

public class NozzleCore : ModuleRules
{
    public NozzleCore(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string PluginRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", ".."));
        string NozzleRoot = Path.Combine(PluginRoot, "ThirdParty", "nozzle");
        string IncludeDirectory = Path.Combine(NozzleRoot, "include");
        string LibraryPath = Path.Combine(NozzleRoot, "lib", "Win64", "nozzle.lib");
        string RuntimeLibraryPath = Path.Combine(NozzleRoot, "bin", "Win64", "nozzle.dll");

        bool HasStagedHeaders = File.Exists(Path.Combine(IncludeDirectory, "nozzle", "nozzle_c.h"));
        bool HasStagedLibrary = File.Exists(LibraryPath);
        bool HasStagedRuntimeLibrary = File.Exists(RuntimeLibraryPath);

        if(Target.Platform == UnrealTargetPlatform.Win64 && HasStagedHeaders && HasStagedLibrary && HasStagedRuntimeLibrary)
        {
            PublicSystemIncludePaths.Add(IncludeDirectory);
            PublicAdditionalLibraries.Add(LibraryPath);
            RuntimeDependencies.Add(RuntimeLibraryPath);
            PublicDefinitions.Add("WITH_NOZZLE_CORE=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_NOZZLE_CORE=0");
        }
    }
}
