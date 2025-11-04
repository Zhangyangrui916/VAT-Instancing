#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include <Engine/AssetUserData.h>
#include "PerInstanceCustomDataLayout.generated.h"

/**
 * 作为Material的User Data Asset, 定义参数名和PerInstanceCustomData索引的映射关系
 */

UCLASS(Blueprintable, BlueprintType)
class VATINSTANCING_API UPerInstanceCustomDataLayout : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (TitleProperty = "ParameterName"))
	TMap<FName, int32> ParameterNameToIndexMap;

	UPROPERTY(EditAnywhere, meta = (EditorOnly = "true"))
	FString Description;

};

UCLASS()
class VATINSTANCING_API UPerInstanceCustomDataLayoutUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UPerInstanceCustomDataLayout* Layout;
};
