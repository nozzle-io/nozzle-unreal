using UnrealBuildTool;

public class NozzleSmoke : ModuleRules
{
    public NozzleSmoke(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Nozzle"
        });
    }
}
