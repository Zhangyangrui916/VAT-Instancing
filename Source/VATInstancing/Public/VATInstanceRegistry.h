#pragma once

#include "VatiDefines.h"
#include "UObject/ObjectPtr.h"
#include "Containers/Map.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IVATInstanceRendererInterface;
class UWorld;
class UObject;

/**
 * A static router class for the VAT instancing system.
 * Its primary role is to route registration and update calls from proxies
 * to the correct renderer instance for the proxy's world.
 * 
 * It is designed to be robust against object lifecycle issues by storing
 * weak pointers to renderers and performing validity checks.
 */
namespace VATInstanceRegistry
{
	// --- Public API ---

	/** Registers a new proxy with the appropriate renderer for its world. */
	void RegisterProxy(UObject* WorldContextObject, FVATProxyId ProxyId, const FBatchKey& BatchKey, const FTransform& InitialTransform, const TArray<float>& InitialCustomData);

	/** Unregisters a proxy from its renderer. */
	void UnregisterProxy(UObject* WorldContextObject, FVATProxyId ProxyId);

	/** Updates the visual state of an existing proxy. */
	void NotifyProxyVisualsChanged(UObject* WorldContextObject, FVATProxyId ProxyId, const FTransform& NewTransform, const TArray<float>& NewCustomData);

	/** Changes the batch key for an existing proxy. */
	void NotifyProxyBatchKeyChanged(UObject* WorldContextObject, FVATProxyId ProxyId, const FBatchKey& NewBatchKey);

	// --- Internal Functions (for Renderers to register/unregister themselves) ---

	/**
	 * Registers a renderer instance for a specific world.
	 * Called by the renderer itself upon initialization.
	 * @param World The world the renderer belongs to.
	 * @param Renderer The renderer instance (must be a UObject that implements IVATInstanceRendererInterface).
	 */
	void RegisterRenderer(UWorld* World, UObject* Renderer);

	/**
	 * Unregisters the renderer for a specific world.
	 * Called by the renderer itself upon deinitialization/destruction.
	 * @param World The world to unregister the renderer for.
	 */
	void UnregisterRenderer(UWorld* World);


	// --- Internal Functions ---

	/** Retrieves the correct renderer for a given world. It does NOT create a renderer. */
	IVATInstanceRendererInterface* GetRendererForWorld(UObject* WorldContextObject);

	/** Provides read-only access to the internal renderer map for debugging purposes. */
	VATINSTANCING_API const TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UObject>>& GetRenderers();

	extern FDelegateHandle OnPostWorldCleanupHandle;
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);


	// --- State ---

	/** A map of all known worlds to their dedicated renderer instances. Stored as weak pointers for safety. */
	extern TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UObject>> Renderers;

}  // namespace VATInstanceRegistry
