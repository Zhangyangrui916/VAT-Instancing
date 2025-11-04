#pragma once

#include "VatiDefines.h"
#include "UObject/Interface.h"
#include "VATInstanceRendererInterface.generated.h"

class UMyAnimToTextureDataAsset;
class AActor;

UINTERFACE(MinimalAPI, Blueprintable)
class UVATInstanceRendererInterface : public UInterface
{
	GENERATED_BODY()
};

class VATINSTANCING_API IVATInstanceRendererInterface
{
	GENERATED_BODY()

public:
	/**
	 * Registers a new proxy with the renderer.
	 * @param ProxyId The unique ID of the proxy to register.
	 * @param BatchKey The initial set of materials and visual assets for the proxy.
	 * @param InitialTransform The starting transform of the proxy.
	 * @param InitialCustomData The starting custom data for the proxy.
	 */
	virtual void RegisterProxy(FVATProxyId ProxyId, const FBatchKey& BatchKey, const FTransform& InitialTransform, const TArray<float>& InitialCustomData) = 0;

	/**
	 * Unregisters a proxy from the renderer, removing its visual instance.
	 * @param ProxyId The unique ID of the proxy to unregister.
	 */
	virtual void UnregisterProxy(FVATProxyId ProxyId) = 0;

	/**
	 * Updates the visual state (transform and custom data) of an existing proxy.
	 * This is expected to be called frequently for animating proxies.
	 * @param ProxyId The unique ID of the proxy to update.
	 * @param NewTransform The new transform of the proxy.
	 * @param NewCustomData The new custom data for the proxy.
	 */
	virtual void UpdateProxyVisuals(FVATProxyId ProxyId, const FTransform& NewTransform, const TArray<float>& NewCustomData) = 0;

	/**
	 * Changes the batch key for a proxy, effectively moving it from one render batch to another.
	 * This is used when materials or the core visual asset change.
	 * @param ProxyId The unique ID of the proxy to update.
	 * @param NewBatchKey The new batch key for the proxy.
	 */
	virtual void UpdateProxyBatchKey(FVATProxyId ProxyId, const FBatchKey& NewBatchKey) = 0;

	/**
	 * Dumps the current state of the renderer to the output log for debugging purposes.
	 * @param Ar The output device to write the log to.
	 */
	virtual void DumpDebugInfo(FOutputDevice& Ar) const = 0;

	/**
	 * Gets the current state of the renderer as a formatted string.
	 * @return FString containing the debug information.
	 */
	virtual FString GetDebugInfoAsString() const = 0;
};