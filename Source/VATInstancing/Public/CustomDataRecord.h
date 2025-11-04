#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CustomDataRecord.generated.h"

USTRUCT()
struct FRecordItem
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	FName Name;
	FTransform Trans;
	float Values[3];

	FRecordItem()
	{
		Name = NAME_None;
		Values[0] = 0.0f;
		Values[1] = 0.0f;
		Values[2] = 0.0f;

	};

	FRecordItem(FName InName, float InValue1, float InValue2, float InValue3, FTransform InTrans)
		: Name(InName)
		, Trans(InTrans)
	{
		Values[0] = InValue1;
		Values[1] = InValue2;
		Values[2] = InValue3;
	};
};


/**
 * 
 */
UCLASS()
class VATINSTANCING_API UCustomDataRecord : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TArray<FRecordItem> Datas;
};
