using UnrealBuildTool;
using System.Collections.Generic;

public class IronvowTarget : TargetRules
{
	public IronvowTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Ironvow");
	}
}
