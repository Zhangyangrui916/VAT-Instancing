#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVATInstancingEditor, Log, All);

class FVatiDebugCollector;

class FVATInstancingEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TUniquePtr<FVatiDebugCollector> VatiDebugCollector;
};
