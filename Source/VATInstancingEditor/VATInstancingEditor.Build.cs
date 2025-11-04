using UnrealBuildTool;

public class VATInstancingEditor : ModuleRules
{
	public VATInstancingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetDefinition",
				"Core",
				"Engine",
				"MaterialEditor",
				"MessageLog"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetTools",
				"ContentBrowser",
				"CoreUObject",
				"Slate",
				"SlateCore",
                "VATInstancing",
				"RawMesh",
				"MeshDescription",
				"StaticMeshDescription",
				"ToolMenus",
				"UnrealEd",
				"AssetRegistry",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
