using UnrealBuildTool;
using System.Collections.Generic;

public class NozzleSmokeTarget : TargetRules
{
    public NozzleSmokeTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.AddRange(new string[] { "NozzleSmoke" });
    }
}
