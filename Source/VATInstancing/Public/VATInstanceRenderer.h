#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "VATInstanceRendererInterface.h"
#include "VATInstanceRenderer.generated.h"

class UInstancedStaticMeshComponent;
class UWorld;

/**
 * A lightweight, UObject-based renderer for VAT instances, designed for editor preview worlds.
 * It implements the core rendering logic using ISMCs and manages its own state.
 * It is responsible for protecting itself from garbage collection and registering/unregistering
 * with the central VATInstanceRegistry.
 */
UCLASS()
class VATINSTANCING_API UVATInstanceRenderer : public UObject, public IVATInstanceRendererInterface
{
	GENERATED_BODY()

public:
	UVATInstanceRenderer();
	virtual ~UVATInstanceRenderer();

	/**
	 * Initializes the renderer with the world it should operate in.
	 * This is the primary entry point after creation.
	 */
	void Init(UWorld* InWorld);

	//~ Begin IVATInstanceRendererInterface
	virtual void RegisterProxy(FVATProxyId ProxyId, const FBatchKey& BatchKey, const FTransform& InitialTransform, const TArray<float>& InitialCustomData) override;
	virtual void UnregisterProxy(FVATProxyId ProxyId) override;
	virtual void UpdateProxyVisuals(FVATProxyId ProxyId, const FTransform& NewTransform, const TArray<float>& NewCustomData) override;
	virtual void UpdateProxyBatchKey(FVATProxyId ProxyId, const FBatchKey& NewBatchKey) override;
	//~ End IVATInstanceRendererInterface

	virtual void DumpDebugInfo(FOutputDevice& Ar) const override;

	virtual FString GetDebugInfoAsString() const override;

protected:
	/** The world this renderer is associated with. Weak pointer to avoid cycles. */
	TWeakObjectPtr<UWorld> OwnerWorld;

	// Information about a single registered proxy instance.
	struct FProxyInstanceInfo
	{
		FBatchKey BatchKey;
		int32 InstanceIndex; // The index in the ISMC's instance buffer.
		TWeakObjectPtr<UInstancedStaticMeshComponent> Ismc;
	};

	// Maps a proxy ID to its instance information.
	TMap<FVATProxyId, FProxyInstanceInfo> InstanceInfos;

	TMap<FBatchKey, TObjectPtr<UInstancedStaticMeshComponent>> Batches;

	// Helper to find or create an ISMC for a given batch key.
	UInstancedStaticMeshComponent* FindOrCreateIsmcForBatch(const FBatchKey& BatchKey);
};
