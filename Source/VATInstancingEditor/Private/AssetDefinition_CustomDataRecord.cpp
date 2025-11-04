#include "AssetDefinition_CustomDataRecord.h"
#include "CustomDataRecord.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_CustomDataRecord"

FText UAssetDefinition_CustomDataRecord::GetAssetDisplayName() const
{
	return LOCTEXT("CustomDataRecordAssetActions", "CustomDataRecord");
}

FLinearColor UAssetDefinition_CustomDataRecord::GetAssetColor() const
{
	return FColor::Green;
}

TSoftClassPtr<UObject> UAssetDefinition_CustomDataRecord::GetAssetClass() const
{
	return UCustomDataRecord::StaticClass();
}

#undef LOCTEXT_NAMESPACE