#pragma once

#include "AssetDefinitionDefault.h"

#include "AssetDefinition_MyAnimToTexture.generated.h"

UCLASS()
class UAssetDefinition_MyAnimToTexture : public UAssetDefinitionDefault
{
	GENERATED_BODY()

protected:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	// UAssetDefinition End
};
