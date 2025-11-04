#include "AssetDefinition_CustomDataLayout.h"
#include "PerInstanceCustomDataLayout.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_CustomDataLayout"

FText UAssetDefinition_CustomDataLayout::GetAssetDisplayName() const
{
	return LOCTEXT("CustomDataLayoutAssetActions", "CustomDataLayout");
}

FLinearColor UAssetDefinition_CustomDataLayout::GetAssetColor() const
{
	return FColor::Green;
}

TSoftClassPtr<UObject> UAssetDefinition_CustomDataLayout::GetAssetClass() const
{
	return UPerInstanceCustomDataLayout::StaticClass();
}

#undef LOCTEXT_NAMESPACE