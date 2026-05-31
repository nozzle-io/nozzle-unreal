using UnrealBuildTool;

public class Nozzle : ModuleRules
{
    public Nozzle(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Engine",
            "NozzleCore",
            "Projects",
            "RenderCore",
            "RHI"
        });

        if(Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("D3D11RHI");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
            PublicDefinitions.Add("NOZZLE_UNREAL_TARGET_WIN64=1");
            PublicDefinitions.Add("NOZZLE_UNREAL_PHASE0_RHI_D3D11=1");
            PublicDefinitions.Add("NOZZLE_UNREAL_D3D11_RUNTIME=1");
        }
        else
        {
            PublicDefinitions.Add("NOZZLE_UNREAL_TARGET_WIN64=0");
            PublicDefinitions.Add("NOZZLE_UNREAL_UNSUPPORTED_PLATFORM=1");
        }
    }
}
