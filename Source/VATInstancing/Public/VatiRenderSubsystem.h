#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VATInstanceRendererInterface.h"
#include "VatiRenderSubsystem.generated.h"

class UInstancedStaticMeshComponent;

/**
 * The main VAT renderer for the game world, implemented as a UWorldSubsystem.
 */
UCLASS()
class VATINSTANCING_API UVatiRenderSubsystem : public UWorldSubsystem, public IVATInstanceRendererInterface
{
	GENERATED_BODY()

public:
	UVatiRenderSubsystem();

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	//~ Begin IVATInstanceRendererInterface
	virtual void RegisterProxy(FVATProxyId ProxyId, const FBatchKey& BatchKey, const FTransform& InitialTransform, const TArray<float>& InitialCustomData) override;
	virtual void UnregisterProxy(FVATProxyId ProxyId) override;
	virtual void UpdateProxyVisuals(FVATProxyId ProxyId, const FTransform& NewTransform, const TArray<float>& NewCustomData) override;
	virtual void UpdateProxyBatchKey(FVATProxyId ProxyId, const FBatchKey& NewBatchKey) override;
	//~ End IVATInstanceRendererInterface

	virtual void DumpDebugInfo(FOutputDevice& Ar) const override;

	virtual FString GetDebugInfoAsString() const override;

protected:
	// Information about a single registered proxy instance.
	struct FProxyInstanceInfo
	{
		FBatchKey BatchKey;
		int32 InstanceIndex; // The index in the ISMC's instance buffer.
		TWeakObjectPtr<UInstancedStaticMeshComponent> Ismc;
	};

	// Maps a proxy ID to its instance information.
	TMap<FVATProxyId, FProxyInstanceInfo> InstanceInfos;

	// Maps a batch key to the corresponding ISMC.
	TMap<FBatchKey, TObjectPtr<UInstancedStaticMeshComponent>> Batches;

	// Helper to find or create an ISMC for a given batch key.
	UInstancedStaticMeshComponent* FindOrCreateIsmcForBatch(const FBatchKey& BatchKey);
};

