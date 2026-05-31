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
        string LibraryPath = "";
        string RuntimeLibraryPath = "";

        if(Target.Platform == UnrealTargetPlatform.Win64)
        {
            LibraryPath = Path.Combine(NozzleRoot, "lib", "Win64", "nozzle.lib");
            RuntimeLibraryPath = Path.Combine(NozzleRoot, "bin", "Win64", "nozzle.dll");
        }
        else if(Target.Platform == UnrealTargetPlatform.Mac)
        {
            LibraryPath = Path.Combine(NozzleRoot, "lib", "Mac", "libnozzle.dylib");
            RuntimeLibraryPath = LibraryPath;
        }

        bool HasStagedHeaders = File.Exists(Path.Combine(IncludeDirectory, "nozzle", "nozzle_c.h"));
        bool HasStagedLibrary = LibraryPath.Length > 0 && File.Exists(LibraryPath);
        bool HasStagedRuntimeLibrary = RuntimeLibraryPath.Length > 0 && File.Exists(RuntimeLibraryPath);

        if((Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac) && HasStagedHeaders && HasStagedLibrary && HasStagedRuntimeLibrary)
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
