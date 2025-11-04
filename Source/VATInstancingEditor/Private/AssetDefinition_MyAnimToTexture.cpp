#include "AssetDefinition_MyAnimToTexture.h"

#include "VATInstancingBPLibrary.h"
#include "MyAnimToTextureDataAsset.h"
#include "AssetViewUtils.h"
#include "ContentBrowserMenuContexts.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ObjectEditorUtils.h"
#include "Styling/SlateIconFinder.h"
#include "ToolMenus.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_MyAnimToTexture"

FText UAssetDefinition_MyAnimToTexture::GetAssetDisplayName() const
{
	return LOCTEXT("AnimToTextureAssetActions", "AnimToTexture");
}

FLinearColor UAssetDefinition_MyAnimToTexture::GetAssetColor() const
{
	return FColor::Blue;
}

TSoftClassPtr<UObject> UAssetDefinition_MyAnimToTexture::GetAssetClass() const
{
	return UMyAnimToTextureDataAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MyAnimToTexture::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::Animation };
	return Categories;
}

// Menu Extensions
//--------------------------------------------------------------------

namespace MenuExtension_AnimToTexture
{
	void RunAnimToTexture(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		TArray<UMyAnimToTextureDataAsset*> SelectedAnimToTextureObjects = Context->LoadSelectedObjects<UMyAnimToTextureDataAsset>();

		for (auto ObjIt = SelectedAnimToTextureObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			UMyAnimToTextureDataAsset* DataAsset = *ObjIt;
			// Create UVs and Textures
			if (UVATInstancingBPLibrary::AnimationToTexture(*ObjIt))
			{
				// Update Material Instances (if Possible)
				if (UStaticMesh* StaticMesh = DataAsset->GetStaticMesh())
				{
					for (FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials())
					{
						if (UMaterialInstanceConstant* MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(StaticMaterial.MaterialInterface))
						{
							UVATInstancingBPLibrary::UpdateMaterialInstanceFromDataAsset(DataAsset, MaterialInstanceConstant, EMaterialParameterAssociation::LayerParameter);
						}
					}
				}
			}
		}
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{ 
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMyAnimToTextureDataAsset::StaticClass());

			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection))
				{
					const TAttribute<FText> Label = LOCTEXT("AnimToTexture_Run", "Run Animation To Texture");
					const TAttribute<FText> ToolTip = LOCTEXT("AnimToTexture_RunTooltip", "Creates Vertex Animation Textures (VAT)");
					const FSlateIcon Icon = FSlateIcon();

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&RunAnimToTexture);

					InSection.AddMenuEntry(TEXT("AnimToTexture_RunAnimationToTexture"), Label, ToolTip, Icon, UIAction);
				}
			}));
		}));
	});
};

//--------------------------------------------------------------------
// Menu Extensions

#undef LOCTEXT_NAMESPACE
