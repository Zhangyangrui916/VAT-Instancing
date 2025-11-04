#include "VATInstanceRegistry.h"
#include "VATInstanceRendererInterface.h"
#include "VatiRenderSubsystem.h"
#include "VATInstanceRenderer.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY(LogVATInstancing);

namespace VATInstanceRegistry
{
	// Define static members
	TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UObject>> Renderers;

	FDelegateHandle OnPostWorldCleanupHandle = FWorldDelegates::OnPostWorldCleanup.AddStatic(OnWorldCleanup);

	void RegisterProxy(UObject* WorldContextObject, FVATProxyId ProxyId, const FBatchKey& BatchKey, const FTransform& InitialTransform, const TArray<float>& InitialCustomData)
	{
		if (IVATInstanceRendererInterface* Renderer = GetRendererForWorld(WorldContextObject))
		{
			Renderer->RegisterProxy(ProxyId, BatchKey, InitialTransform, InitialCustomData);
		}
	}

	void UnregisterProxy(UObject* WorldContextObject, FVATProxyId ProxyId)
	{
		// It's crucial that we check the renderer is valid here.
		// This function is often called during destruction, at which point the renderer might already be gone.
		if (IVATInstanceRendererInterface* Renderer = GetRendererForWorld(WorldContextObject))
		{
			Renderer->UnregisterProxy(ProxyId);
		}
	}

	void NotifyProxyVisualsChanged(UObject* WorldContextObject, FVATProxyId ProxyId, const FTransform& NewTransform, const TArray<float>& NewCustomData)
	{
		if (IVATInstanceRendererInterface* Renderer = GetRendererForWorld(WorldContextObject))
		{
			Renderer->UpdateProxyVisuals(ProxyId, NewTransform, NewCustomData);
		}
	}

	void NotifyProxyBatchKeyChanged(UObject* WorldContextObject, FVATProxyId ProxyId, const FBatchKey& NewBatchKey)
	{
		if (IVATInstanceRendererInterface* Renderer = GetRendererForWorld(WorldContextObject))
		{
			Renderer->UpdateProxyBatchKey(ProxyId, NewBatchKey);
		}
	}

	void RegisterRenderer(UWorld* World, UObject* Renderer)
	{
		if (!World || !Renderer)
		{
			return;
		}

		if (!Cast<IVATInstanceRendererInterface>(Renderer))
		{
			UE_LOG(LogVATInstancing, Error, TEXT("VATInstanceRegistry: Attempted to register a renderer that does not implement IVATInstanceRendererInterface."));
			return;
		}

		if (Renderers.Contains(World))
		{
			UE_LOG(LogVATInstancing, Warning, TEXT("VATInstanceRegistry: Overwriting existing renderer for world %s."), *World->GetName());
		}

		Renderers.Add(World, Renderer);
		UE_LOG(LogVATInstancing, Log, TEXT("VATInstanceRegistry: Registered renderer for world %s."), *World->GetName());
	}

	void UnregisterRenderer(UWorld* World)
	{
		if (World && Renderers.Contains(World))
		{
#if WITH_EDITOR
			Renderers[World]->RemoveFromRoot();
#endif
			Renderers.Remove(World);
			UE_LOG(LogVATInstancing, Log, TEXT("VATInstanceRegistry: Unregistered renderer for world %s."), *World->GetName());
		}
	}

	IVATInstanceRendererInterface* GetRendererForWorld(UObject* WorldContextObject)
	{
		if (!WorldContextObject) return nullptr;
		UWorld* World = WorldContextObject->GetWorld();
		if (!World || World->bIsTearingDown || World->WorldType == EWorldType::Inactive)
		{
			return nullptr;
		}

		//// Clean up any stale entries first. This makes the registry self-healing.
		//for (auto It = Renderers.CreateIterator(); It; ++It)
		//{
		//	if (!It.Key().IsValid() || !It.Value().IsValid())
		//	{
		//		UE_LOG(LogVATInstancing, Log, TEXT("VATInstanceRegistry: Cleaned up stale renderer entry."));
		//		It.RemoveCurrent();
		//	}
		//}

		// First, try to find an existing renderer for this world.
		if (TWeakObjectPtr<UObject>* RendererPtr = Renderers.Find(World))
		{
			if (RendererPtr->IsValid())
			{
				return Cast<IVATInstanceRendererInterface>(RendererPtr->Get());
			}
		}

		// If no renderer was found, determine which type to use.
		// Case 1: Game worlds and PIE worlds use the subsystem.
		if (World->IsGameWorld())
		{
			// The subsystem registers itself, so it should already be in the map.
			// If not, the subsystem might not have initialized yet. We can get it directly.
			return World->GetSubsystem<UVatiRenderSubsystem>();
		}
#if WITH_EDITOR
		// Case 2: Pure editor worlds (e.g., asset previews) need a Swap renderer.
		else if (World->IsEditorWorld()) // This is true for Editor, Preview, and PIE. IsGameWorld check already handled PIE.
		{
			// Create a new renderer for this preview world.
			// This is safe because UVATInstanceRenderer manages its own lifecycle via AddToRoot.
			UE_LOG(LogVATInstancing, Log, TEXT("Creating new UVATInstanceRenderer for world: %s"), *World->GetName());
			UVATInstanceRenderer* NewRenderer = NewObject<UVATInstanceRenderer>();
			NewRenderer->Init(World); // This will call AddToRoot() and RegisterRenderer()
			return NewRenderer;
		}
#endif

		return nullptr;
	}

	const TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UObject>>& GetRenderers()
	{
		return Renderers;
	}

	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		Renderers.Remove(World);
	}
} // namespace VATInstanceRegistry

