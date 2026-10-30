using UnrealBuildTool;
using System.Collections.Generic;

public class IronvowEditorTarget : TargetRules
{
	public IronvowEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Ironvow");
	}
}
