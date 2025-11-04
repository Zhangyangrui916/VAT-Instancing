#include "VATInstancingEditorModule.h"
#include "VatiDebugCollector.h"

#include "AssetToolsModule.h"
#include "MessageLogModule.h"

#define LOCTEXT_NAMESPACE "VATInstancingEditor"

DEFINE_LOG_CATEGORY(LogVATInstancingEditor);

void FVATInstancingEditorModule::StartupModule()
{
	// Register Log
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;
	InitOptions.bShowPages = false;
	InitOptions.bAllowClear = true;
	MessageLogModule.RegisterLogListing("AnimToTextureLog", LOCTEXT("AnimToTextureLog", "AnimToTexture Log"), InitOptions);

	//VatiDebugCollector = MakeUnique<FVatiDebugCollector>();
}

void FVATInstancingEditorModule::ShutdownModule()
{
	//VatiDebugCollector.Reset();

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.UnregisterLogListing("AnimToTextureLog");
}

IMPLEMENT_MODULE(FVATInstancingEditorModule, VATInstancingEditor)

#undef LOCTEXT_NAMESPACE
