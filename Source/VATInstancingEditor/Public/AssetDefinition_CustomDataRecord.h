#pragma once

#include "AssetDefinitionDefault.h"

#include "AssetDefinition_CustomDataRecord.generated.h"

UCLASS()
class UAssetDefinition_CustomDataRecord : public UAssetDefinitionDefault
{
	GENERATED_BODY()

protected:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	// UAssetDefinition End
};
