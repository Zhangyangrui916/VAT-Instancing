#include "VatiRenderSubsystem.h"
#include "VATInstanceRegistry.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "MyAnimToTextureDataAsset.h"

UVatiRenderSubsystem::UVatiRenderSubsystem()
{
}

void UVatiRenderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Register this subsystem as the official game renderer for this world.
	VATInstanceRegistry::RegisterRenderer(GetWorld(), this);
}

void UVatiRenderSubsystem::Deinitialize()
{
	// Unregister this subsystem. This is crucial to prevent access after destruction.
	VATInstanceRegistry::UnregisterRenderer(GetWorld());

	// Clean up all created ISMC components.
	for (auto& It : Batches)
	{
		if (It.Value)
		{
			It.Value->RemoveFromRoot();
			It.Value->DestroyComponent();
		}
	}
	Batches.Empty();
	InstanceInfos.Empty();

	Super::Deinitialize();
}

void UVatiRenderSubsystem::RegisterProxy(FVATProxyId ProxyId, const FBatchKey& BatchKey, const FTransform& InitialTransform, const TArray<float>& InitialCustomData)
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

void UVatiRenderSubsystem::UnregisterProxy(FVATProxyId ProxyId)
{
	FProxyInstanceInfo Info;
	if (!InstanceInfos.RemoveAndCopyValue(ProxyId, Info))
	{
		return;
	}

	UInstancedStaticMeshComponent* Ismc = Info.Ismc.Get();
	if (!Ismc) return;

	const int32 IndexToRemove = Info.InstanceIndex;

	const int32 LastIndex = Ismc->GetInstanceCount() - 1;

	if (IndexToRemove < LastIndex)
	{
		// Find the actor that was at the last index
		FVATProxyId LastId = InvalidVATProxyId;
		for (auto It = InstanceInfos.CreateConstIterator(); It; ++It)
		{
			if (It->Value.Ismc == Ismc && It->Value.InstanceIndex == LastIndex)
			{
				LastId = It->Key;
				break;
			}
		}


		// Get transform and custom data from the last instance
		FTransform LastTransform;
		Ismc->GetInstanceTransform(LastIndex, LastTransform, false);
		Ismc->UpdateInstanceTransform(IndexToRemove, LastTransform, false, false, true);

		// 替换原来的 SetCustomData 调用为直接 memcpy
		const int32 NumFloats = Ismc->NumCustomDataFloats;
		float* SrcCustomData = Ismc->PerInstanceSMCustomData.GetData() + LastIndex * NumFloats;
		float* DestCustomData = Ismc->PerInstanceSMCustomData.GetData() + IndexToRemove * NumFloats;
		FMemory::Memcpy(DestCustomData, SrcCustomData, NumFloats * sizeof(float));

		// Update the map for the actor that was moved
		auto& LastInstanceInfo = InstanceInfos.FindChecked(LastId);
		LastInstanceInfo.InstanceIndex = IndexToRemove;
	}

	// Remove the last instance, which is now either redundant or the one we intended to remove anyway.
	ensure(Ismc->GetInstanceCount() > 0);
	Ismc->RemoveInstance(LastIndex);


}

void UVatiRenderSubsystem::UpdateProxyVisuals(FVATProxyId ProxyId, const FTransform& NewTransform, const TArray<float>& NewCustomData)
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

void UVatiRenderSubsystem::UpdateProxyBatchKey(FVATProxyId ProxyId, const FBatchKey& NewBatchKey)
{
	FProxyInstanceInfo* Info = InstanceInfos.Find(ProxyId);
	if (!Info || Info->BatchKey == NewBatchKey)
	{
		return; // No change needed
	}

	FTransform CurrentTransform = FTransform::Identity;
	TArray<float> CurrentCustomData; // This data will be lost and needs re-populating.

	if (UInstancedStaticMeshComponent* OldIsmc = Info->Ismc.Get())
	{
		OldIsmc->GetInstanceTransform(Info->InstanceIndex, CurrentTransform);
	}

	// Unregister from the old batch and re-register with the new one.
	UnregisterProxy(ProxyId);
	RegisterProxy(ProxyId, NewBatchKey, CurrentTransform, CurrentCustomData);
}

UInstancedStaticMeshComponent* UVatiRenderSubsystem::FindOrCreateIsmcForBatch(const FBatchKey& BatchKey)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* FoundIsmc = Batches.Find(BatchKey))
	{
		return *FoundIsmc;
	}

	UWorld* World = GetWorld();
	if (!World || !BatchKey.VisualTypeAsset || !BatchKey.VisualTypeAsset->GetStaticMesh())
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* NewIsmc = NewObject<UInstancedStaticMeshComponent>(this);
	NewIsmc->SetStaticMesh(BatchKey.VisualTypeAsset->GetStaticMesh());
	NewIsmc->NumCustomDataFloats = BatchKey.VisualTypeAsset->NumCustomDataFloatsForVAT;

	for (int32 i = 0; i < BatchKey.BaseMaterials.Num(); ++i)
	{
		NewIsmc->SetMaterial(i, BatchKey.BaseMaterials[i]);
	}
	if (BatchKey.OverlayMaterial)
	{
		// This assumes the overlay is on a specific material index, which needs to be defined by the project's standards.
		// For now, let's assume it's the last material index.
		NewIsmc->SetMaterial(BatchKey.BaseMaterials.Num(), BatchKey.OverlayMaterial);
	}

	NewIsmc->RegisterComponentWithWorld(World);
	NewIsmc->AddToRoot();
	Batches.Add(BatchKey, NewIsmc);
	return NewIsmc;
}

void UVatiRenderSubsystem::DumpDebugInfo(FOutputDevice& Ar) const
{
	Ar.Logf(TEXT("  |- Renderer Type: UVatiRenderSubsystem"));
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

FString UVatiRenderSubsystem::GetDebugInfoAsString() const
{
	FStringOutputDevice Ar;
	DumpDebugInfo(Ar);
	return Ar;
}


