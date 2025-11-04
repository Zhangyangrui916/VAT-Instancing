#include "VATInstancingModule.h"

#include "Logging/LogMacros.h"

#define LOCTEXT_NAMESPACE "FVATInstancingModule"

void FVATInstancingModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogTemp, Log, TEXT("VATInstancing module has started."));
}

void FVATInstancingModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UE_LOG(LogTemp, Log, TEXT("VATInstancing module has shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVATInstancingModule, VATInstancing)