#include "VATInstanceRenderer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "MyAnimToTextureDataAsset.h"
#include "VATInstanceRegistry.h"
#include "Materials/MaterialInstanceDynamic.h"

UVATInstanceRenderer::UVATInstanceRenderer()
{
	// Protect this renderer from being garbage collected prematurely.
	// The creator of this renderer is now responsible for its destruction.
	AddToRoot();
}

UVATInstanceRenderer::~UVATInstanceRenderer()
{
	// Unregister from the central registry.
	if (OwnerWorld.IsValid())
	{
		VATInstanceRegistry::UnregisterRenderer(OwnerWorld.Get());
	}
	//else
	//{
	//	ensure(false);
	//}

	// This object is being destroyed, so we need to remove it from the root set
	// to allow the garbage collector to reclaim its memory.
	if (IsRooted())
	{
		RemoveFromRoot();
	}

	// Clean up all created ISMC components.
	for (auto& It : Batches)
	{
		if (It.Value)
		{
			It.Value->DestroyComponent();
			It.Value->RemoveFromRoot();
		}
	}
	Batches.Empty();
}

void UVATInstanceRenderer::Init(UWorld* InWorld)
{
	if (!InWorld) return;

	OwnerWorld = InWorld;

	// Register this instance with the central registry.
	VATInstanceRegistry::RegisterRenderer(InWorld, this);
}

void UVATInstanceRenderer::RegisterProxy(FVATProxyId ProxyId, const FBatchKey& BatchKey, const FTransform& InitialTransform, const TArray<float>& InitialCustomData)
{
	if (InstanceInfos.Contains(ProxyId))
	{
		UpdateProxyBatchKey(ProxyId, BatchKey);
		UpdateProxyVisuals(ProxyId, InitialTransform, InitialCustomData);
		return;
	}

	UInstancedStaticMeshComponent* Ismc = FindOrCreateIsmcForBatch(BatchKey);
	if (!Ismc) return;

	const int32 InstanceIndex = Ismc->AddInstance(InitialTransform);
	Ismc->SetCustomData(InstanceIndex, InitialCustomData);

	InstanceInfos.Add(ProxyId, { BatchKey, InstanceIndex, Ismc });
}

void UVATInstanceRenderer::UnregisterProxy(FVATProxyId ProxyId)
{
	FProxyInstanceInfo Info;
	if (!InstanceInfos.RemoveAndCopyValue(ProxyId, Info))
	{
		return;
	}

	UInstancedStaticMeshComponent* Ismc = Info.Ismc.Get();
	if (!Ismc || Ismc->GetInstanceCount() == 0)
	{
		return;
	}

	const int32 IndexToRemove = Info.InstanceIndex;
	Ismc->RemoveInstance(IndexToRemove);
}

void UVATInstanceRenderer::UpdateProxyVisuals(FVATProxyId ProxyId, const FTransform& NewTransform, const TArray<float>& NewCustomData)
{
	if (FProxyInstanceInfo* Info = InstanceInfos.Find(ProxyId))
	{
		if (UInstancedStaticMeshComponent* Ismc = Info->Ismc.Get())
		{
			Ismc->UpdateInstanceTransform(Info->InstanceIndex, NewTransform, true, true, false);
			Ismc->SetCustomData(Info->InstanceIndex, NewCustomData);
		}
	}
}

void UVATInstanceRenderer::UpdateProxyBatchKey(FVATProxyId ProxyId, const FBatchKey& NewBatchKey)
{
	FProxyInstanceInfo* Info = InstanceInfos.Find(ProxyId);
	if (!Info || Info->BatchKey == NewBatchKey)
	{
		return; // No change needed
	}

	FTransform CurrentTransform = FTransform::Identity;
	TArray<float> CurrentCustomData;

	if (UInstancedStaticMeshComponent* OldIsmc = Info->Ismc.Get())
	{
		OldIsmc->GetInstanceTransform(Info->InstanceIndex, CurrentTransform);
	}
	
	UnregisterProxy(ProxyId);
	RegisterProxy(ProxyId, NewBatchKey, CurrentTransform, CurrentCustomData);
}


UInstancedStaticMeshComponent* UVATInstanceRenderer::FindOrCreateIsmcForBatch(const FBatchKey& BatchKey)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* FoundIsmc = Batches.Find(BatchKey))
	{
		return *FoundIsmc;
	}

	UWorld* World = OwnerWorld.Get();
	if (!World || !BatchKey.VisualTypeAsset || !BatchKey.VisualTypeAsset->GetStaticMesh())
	{
		return nullptr;
	}

	// The ISMC is owned by this renderer instance.
	UInstancedStaticMeshComponent* NewIsmc = NewObject<UInstancedStaticMeshComponent>(this);
	NewIsmc->SetStaticMesh(BatchKey.VisualTypeAsset->GetStaticMesh());
	NewIsmc->NumCustomDataFloats = BatchKey.VisualTypeAsset->NumCustomDataFloatsForVAT;

	for (int32 i = 0; i < BatchKey.BaseMaterials.Num(); ++i)
	{
		NewIsmc->SetMaterial(i, BatchKey.BaseMaterials[i]);
	}
	if (BatchKey.OverlayMaterial)
	{
		NewIsmc->SetMaterial(BatchKey.BaseMaterials.Num(), BatchKey.OverlayMaterial);
	}

	NewIsmc->RegisterComponentWithWorld(World);
	NewIsmc->AddToRoot();
	Batches.Add(BatchKey, NewIsmc);
	return NewIsmc;
}

void UVATInstanceRenderer::DumpDebugInfo(FOutputDevice& Ar) const
{
	Ar.Logf(TEXT("  |- Renderer Type: UVATInstanceRenderer"));
	Ar.Logf(TEXT("  |- Total ISMC Batches: %d"), Batches.Num());
	Ar.Logf(TEXT("  |- Total Tracked Instances: %d"), InstanceInfos.Num());

	int32 BatchIndex = 0;
	for (const auto& Elem : Batches)
	{
		const FBatchKey& Key = Elem.Key;
		const TObjectPtr<UInstancedStaticMeshComponent> ISMC = Elem.Value;

		if (ISMC)
		{
			Ar.Logf(TEXT("    |-- Batch [%d]:"), BatchIndex++);
			Ar.Logf(TEXT("    |   - Mesh: %s"), *GetNameSafe(Key.VisualTypeAsset.Get()));
			Ar.Logf(TEXT("    |   - ISMC Instance Count: %d"), ISMC->GetInstanceCount());
		}
	}
}

FString UVATInstanceRenderer::GetDebugInfoAsString() const
{
	FStringOutputDevice Ar;
	DumpDebugInfo(Ar);
	return Ar;
}
