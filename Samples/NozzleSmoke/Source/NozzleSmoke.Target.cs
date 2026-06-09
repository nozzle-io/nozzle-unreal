using UnrealBuildTool;
using System.Collections.Generic;

public class NozzleSmokeTarget : TargetRules
{
    public NozzleSmokeTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        CppStandard = CppStandardVersion.Cpp20;
        ExtraModuleNames.AddRange(new string[] { "NozzleSmoke" });
    }
}
