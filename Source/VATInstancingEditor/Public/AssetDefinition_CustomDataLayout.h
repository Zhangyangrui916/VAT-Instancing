// Copyright Xverse. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "AssetDefinition_CustomDataLayout.generated.h"

/**
 * 
 */
UCLASS()
class VATINSTANCINGEDITOR_API UAssetDefinition_CustomDataLayout : public UAssetDefinitionDefault
{
	GENERATED_BODY()

protected:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	// UAssetDefinition End
};
