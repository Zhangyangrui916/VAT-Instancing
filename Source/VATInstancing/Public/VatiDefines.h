#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"

class UMyAnimToTextureDataAsset;
class UMaterialInterface;

DECLARE_LOG_CATEGORY_EXTERN(LogVATInstancing, Log, All);

// A unique identifier for a proxy instance within the VAT system.
// Can be changed to a smaller type like uint32 if the max concurrent instance count is known to be lower.
using FVATProxyId = uint64;

// A reserved value indicating an invalid or uninitialized proxy ID.
const FVATProxyId InvalidVATProxyId = 0;

struct FBatchKey
{
	TObjectPtr<UMyAnimToTextureDataAsset> VisualTypeAsset;
	TArray<TObjectPtr<UMaterialInterface>> BaseMaterials;
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	FBatchKey()
		: VisualTypeAsset(nullptr)
		, OverlayMaterial(nullptr)
	{
	}

	FBatchKey(UMyAnimToTextureDataAsset* InVisualType,
			  const TArray<TObjectPtr<UMaterialInterface>>& InBaseMaterials,  // 使用TObjectPtr数组传入
			  UMaterialInterface* InOverlayMat)
		: VisualTypeAsset(InVisualType)
		, OverlayMaterial(InOverlayMat)
	{
		// 将TObjectPtr转换为TWeakObjectPtr
		BaseMaterials.Reserve(InBaseMaterials.Num());
		for (const auto& Mat : InBaseMaterials)
		{
			BaseMaterials.Add(Mat);
		}
	}

	bool operator==(const FBatchKey& Other) const
	{
		return VisualTypeAsset == Other.VisualTypeAsset && OverlayMaterial == Other.OverlayMaterial && BaseMaterials == Other.BaseMaterials;
	}

	friend uint32 GetTypeHash(const FBatchKey& Key)
	{
		uint32 Hash = GetTypeHash(Key.VisualTypeAsset);
		Hash = HashCombine(Hash, GetTypeHash(Key.OverlayMaterial));
		// 为数组计算哈希值
		for (const auto& Mat : Key.BaseMaterials)
		{
			Hash = HashCombine(Hash, GetTypeHash(Mat));
		}
		return Hash;
	}
};

#if !UE_BUILD_SHIPPING
	#define ENABLE_REGISTRY_LOGGING 0
#endif