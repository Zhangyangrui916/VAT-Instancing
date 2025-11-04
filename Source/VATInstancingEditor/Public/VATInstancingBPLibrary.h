#pragma once

#include "MyAnimToTextureDataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Materials/MaterialLayersFunctions.h"

#include "VATInstancingBPLibrary.generated.h"

class UStaticMesh;
class USkeletalMesh;


// EUW_VAT_Utils.uasset会调用下面的函数，完成Skeletonmesh到StaticMesh的转换、储存动画信息到texture等功能。
UCLASS()
class UVATInstancingBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	* Bakes Animation Data into Textures.
	*/
	UFUNCTION(BlueprintCallable, meta = (Category = "AnimToTexture"))
	static UPARAM(DisplayName="bSuccess") bool AnimationToTexture(UMyAnimToTextureDataAsset* DataAsset);

	/** 
	* Utility for converting SkeletalMesh into a StaticMesh
	*/
	UFUNCTION(BlueprintCallable, Category = "AnimToTexture")
	static UStaticMesh* ConvertSkeletalMeshToStaticMesh(USkeletalMesh* SkeletalMesh, const FString PackageName, const int32 LODIndex = -1);

	/**
	* Updates a material's parameters to match those of an AnimToTexture DataAsset
	*/
	UFUNCTION(BlueprintCallable, meta = (Category = "AnimToTexture"))
	static void UpdateMaterialInstanceFromDataAsset(UMyAnimToTextureDataAsset* DataAsset, class UMaterialInstanceConstant* MaterialInstance,
		const EMaterialParameterAssociation MaterialParameterAssociation = EMaterialParameterAssociation::LayerParameter);


	UFUNCTION(Exec, Category = "VAT Instancing Debug")
	static void VatiDumpRegistryState();

	UFUNCTION(BlueprintCallable, meta = (Category = "AnimToTexture"))
	static UPARAM(DisplayName="Success Count") int32 BatchUpdateAnimToTextureAssets(TArray<FString>& FailedAssets);
};
