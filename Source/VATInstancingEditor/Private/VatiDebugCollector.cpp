#include "VatiDebugCollector.h"
#include "VATInstanceRegistry.h"
#include "VATInstanceRendererInterface.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "VisualLogger/VisualLogger.h"

// Define a our own Log category to be able to filter in Visual Logger
DEFINE_LOG_CATEGORY_STATIC(LogVatiDebug, Log, All);

// Helper function to get the string representation of WorldType
static FString GetWorldTypeAsString(EWorldType::Type WorldType)
{
    switch (WorldType)
    {
    case EWorldType::None:
        return TEXT("None");
    case EWorldType::Game:
        return TEXT("Game");
    case EWorldType::Editor:
        return TEXT("Editor");
    case EWorldType::PIE:
        return TEXT("PIE");
    case EWorldType::EditorPreview:
        return TEXT("EditorPreview");
    case EWorldType::GamePreview:
        return TEXT("GamePreview");
    case EWorldType::GameRPC:
        return TEXT("GameRPC");
    case EWorldType::Inactive:
        return TEXT("Inactive");
    default:
        return TEXT("Unknown");
    }
}


FVatiDebugCollector::FVatiDebugCollector()
{
}

FVatiDebugCollector::~FVatiDebugCollector()
{
}

TStatId FVatiDebugCollector::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(FVatiDebugCollector, STATGROUP_Tickables);
}

void FVatiDebugCollector::Tick(float DeltaTime)
{
    // Log every 0.2 seconds
    if (FPlatformTime::Seconds() - LastLogTime < 0.2)
    {
        return;
    }
    LastLogTime = FPlatformTime::Seconds();

    const TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UObject>>& Renderers = VATInstanceRegistry::GetRenderers();

    // Use the first valid world as the VLog "Owner" so the log appears in the world context
    UWorld* VLogOwner = nullptr;
    for (const auto& Elem : Renderers)
    {
        if (Elem.Key.IsValid())
        {
            VLogOwner = Elem.Key.Get();
            break;
        }
    }

    FString FullDebugString = FString::Printf(TEXT("Total Tracked Worlds: %d"), Renderers.Num());

    for (const auto& Elem : Renderers)
    {
        const UWorld* World = Elem.Key.Get();
        UObject* RendererObject = Elem.Value.Get();

        if (World && RendererObject)
        {
            if (IVATInstanceRendererInterface* Renderer = Cast<IVATInstanceRendererInterface>(RendererObject))
            {
				FString WorldTypeStr = GetWorldTypeAsString(World->WorldType);
                FullDebugString += FString::Printf(TEXT("\n- World: %s (Type: %s)"), *World->GetName(), *WorldTypeStr);
                FullDebugString += TEXT("\n") + Renderer->GetDebugInfoAsString();
            }
        }
    }
    
    // Send the log to the Visual Logger
    UE_VLOG_UELOG(VLogOwner, LogVatiDebug, Log, TEXT("%s"), *FullDebugString);
}