#include "VATInstancingBPLibrary.h"
#include "VATInstancingEditorModule.h"
#include "AnimToTextureUtils.h"
#include "AnimToTextureSkeletalMesh.h"
#include "PerInstanceCustomDataLayout.h"
#include "MyAnimToTextureDataAsset.h"
#include "RawMesh.h"
#include "BakingUtil.h"
#include "VATMaterialParameterName.h"
#include "MeshUtilities.h"
#include "Modules/ModuleManager.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Math/Vector.h"
#include "AnimToTextureMeshMapping.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditingLibrary.h"
#include "StaticMeshAttributes.h"
#include "Misc/ScopedSlowTask.h"
#include "VATInstanceRegistry.h"
#include "VATInstanceRendererInterface.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "AnimToTextureEditor"

using namespace AnimToTexture_Private;

static bool WriteSkinWeightsToColorAndBoneIdToUvChannel(TArray<TVertexSkinWeight<4>>& SkinWeights, UMyAnimToTextureDataAsset* DataAsset);

bool UVATInstancingBPLibrary::AnimationToTexture(UMyAnimToTextureDataAsset* DataAsset)
{
	if (!DataAsset)
	{
		return false;
	}

	// Runs some checks for the assets in DataAsset
	int32 SocketIndex = INDEX_NONE;
	if (!CheckDataAsset(DataAsset, SocketIndex))
	{
		return false;
	}

	// Reset DataAsset Info Values
	DataAsset->ResetInfo();

	// ---------------------------------------------------------------------------	
	// Get Mapping between Static and Skeletal Meshes
	// Since they might not have same number of points.
	//
	FSourceMeshToDriverMesh Mapping;
	{
		FScopedSlowTask ProgressBar(1.f, LOCTEXT("ProcessingMapping", "Processing StaticMesh -> SkeletalMesh Mapping ..."), true /*Enabled*/);
		ProgressBar.MakeDialog(false /*bShowCancelButton*/, false /*bAllowInPIE*/);

		Mapping.Update(DataAsset->GetStaticMesh(), DataAsset->StaticLODIndex,
			DataAsset->GetSkeletalMesh(), DataAsset->SkeletalLODIndex, DataAsset->NumDriverTriangles, DataAsset->Sigma);
	}

	// Get Number of Source Vertices (StaticMesh)
	const int32 NumVertices = Mapping.GetNumSourceVertices();
	if (!NumVertices)
	{
		return false;
	}

	// ---------------------------------------------------------------------------
	// Get Reference Skeleton Transforms
	//
	TArray<FVector3f> BoneRefPositions;
	TArray<FVector4f> BoneRefRotations_NoUse;

	TArray<FVector3f> BonePositions;
	TArray<FVector4f> BoneRotations;
	
	DataAsset->NumBones = GetRefBonePositionsAndRotations(DataAsset->GetSkeletalMesh(), BoneRefPositions, BoneRefRotations_NoUse);


	// --------------------------------------------------------------------------

	// Create Temp Actor
	check(GEditor);
	UWorld* World = GEditor->GetEditorWorldContext().World();
	check(World);

	AActor* Actor = World->SpawnActor<AActor>();
	check(Actor);

	// Create Temp SkeletalMesh Component
	USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(Actor);
	SkeletalMeshComponent->SetSkeletalMesh(DataAsset->GetSkeletalMesh());
	SkeletalMeshComponent->SetForcedLOD(1); // Force to LOD0;
	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkeletalMeshComponent->SetUpdateAnimationInEditor(true);
	SkeletalMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	SkeletalMeshComponent->RegisterComponent();

	// ---------------------------------------------------------------------------
	// Get Vertex Data (for all frames)
	//		
	TArray<FVector3f> VertexDeltas;
	TArray<FVector3f> VertexNormals;


	TMap<int32, int32> BoneId2InterestListId;
	TMap<FName, int32> SocketName2InterestListId;
	for (int j = 0; j < DataAsset->BoneOrSocketsOfInterestForAllAnimSequences.Num(); ++j)
	{
		auto BoneOrSocketName = DataAsset->BoneOrSocketsOfInterestForAllAnimSequences[j];
		if (int32 BoneIndex = SkeletalMeshComponent->GetBoneIndex(BoneOrSocketName); BoneIndex != INDEX_NONE)
		{
			BoneId2InterestListId.Add(BoneIndex, j);
		}
		else if (SkeletalMeshComponent->GetSocketByName(BoneOrSocketName))
		{
			SocketName2InterestListId.Add(BoneOrSocketName, j);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Not a bone name or socket name"));
		}
	}
	
	// Get Animation Frames Data
	TArray<FAnim2TextureAnimSequenceInfo>& AnimSequences = DataAsset->AnimSequences;
	for (int32 AnimSequenceIndex = 0; AnimSequenceIndex < AnimSequences.Num(); AnimSequenceIndex++)
	{
		FAnim2TextureAnimSequenceInfo& AnimSequenceInfo = AnimSequences[AnimSequenceIndex];

		// Set Animation
		UAnimSequence* AnimSequence = AnimSequenceInfo.AnimSequence;
		SkeletalMeshComponent->SetAnimation(AnimSequence);

		// Get Number of Frames
		int32 AnimStartFrame;
		int32 AnimEndFrame;
		const int32 AnimNumFrames = GetAnimationFrameRange(AnimSequenceInfo, AnimStartFrame, AnimEndFrame);
		const float AnimStartTime = AnimSequence->GetTimeAtFrame(AnimStartFrame);

		// 缓存部分骨骼的Transform到CPU侧。
		const int32 TotalNum = AnimSequenceInfo.BoneOrSocketsOfInterest.Num() + DataAsset->BoneOrSocketsOfInterestForAllAnimSequences.Num();
		AnimSequenceInfo.BoneComponentSpaceTransforms.SetNumUninitialized(TotalNum * AnimNumFrames);
		TMap<int32, int32> BoneId2InterestListIdThisAnim = BoneId2InterestListId;
		TMap<FName, int32> SocketName2InterestListIdThisAnim = SocketName2InterestListId;
		const int32 Offset = DataAsset->BoneOrSocketsOfInterestForAllAnimSequences.Num();
		for (int j = 0; j < AnimSequenceInfo.BoneOrSocketsOfInterest.Num(); ++j)
		{
			auto BoneOrSocketName = AnimSequenceInfo.BoneOrSocketsOfInterest[j];
			if (int32 BoneIndex = SkeletalMeshComponent->GetBoneIndex(BoneOrSocketName); BoneIndex != INDEX_NONE)
			{
				BoneId2InterestListIdThisAnim.Add(BoneIndex, Offset + j);
			}
			else if (SkeletalMeshComponent->GetSocketByName(BoneOrSocketName))
			{
				SocketName2InterestListIdThisAnim.Add(BoneOrSocketName, Offset + j);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("Not a bone name or socket name"));
			}
		}


		int32 SampleIndex = 0;
		const float SampleInterval = 1.f / DataAsset->SampleRate;

		// Progress Bar
		FFormatNamedArguments Args;
		Args.Add(TEXT("AnimSequenceIndex"), AnimSequenceIndex+1);
		Args.Add(TEXT("NumAnimSequences"), AnimSequences.Num());
		Args.Add(TEXT("AnimSequence"), FText::FromString(*AnimSequence->GetFName().ToString()));
		FScopedSlowTask AnimProgressBar(AnimNumFrames, FText::Format(LOCTEXT("ProcessingAnimSequence", "Processing AnimSequence: {AnimSequence} [{AnimSequenceIndex}/{NumAnimSequences}]"), Args), true /*Enabled*/);
		AnimProgressBar.MakeDialog(false /*bShowCancelButton*/, false /*bAllowInPIE*/);

		while (SampleIndex < AnimNumFrames)
		{
			AnimProgressBar.EnterProgressFrame();

			const float Time = AnimStartTime + (static_cast<float>(SampleIndex) * SampleInterval);
			
			SkeletalMeshComponent->SetPosition(Time);
			SkeletalMeshComponent->TickAnimation(0.f, false /*bNeedsValidRootMotion*/);
			SkeletalMeshComponent->RefreshBoneTransforms(nullptr /*TickFunction*/);
			

			if (DataAsset->Mode == EAnim2TextureMode::Vertex)
			{
				TArray<FVector3f> VertexFrameDeltas;
				TArray<FVector3f> VertexFrameNormals;
				
				GetVertexDeltasAndNormals(SkeletalMeshComponent, DataAsset->SkeletalLODIndex,
					Mapping, DataAsset->RootTransform,
					VertexFrameDeltas, VertexFrameNormals);
					
				VertexDeltas.Append(VertexFrameDeltas);
				VertexNormals.Append(VertexFrameNormals);
			}

			// 假如需要将感兴趣的骨骼和Socket的ComponentSpaceTransform存储，那么即使是vertex模式也得执行GetBonePositionsAndRotations
			if (DataAsset->Mode == EAnim2TextureMode::Bone || TotalNum > 0)
			{
				TArray<FVector3f> BoneFramePositions;
				TArray<FVector4f> BoneFrameRotations;

				GetBonePositionsAndRotations(SkeletalMeshComponent, BoneRefPositions, BoneFramePositions, BoneFrameRotations, SampleIndex, AnimSequenceInfo, Offset,
											 BoneId2InterestListIdThisAnim,
											 SocketName2InterestListIdThisAnim);

				BonePositions.Append(BoneFramePositions);
				BoneRotations.Append(BoneFrameRotations);
			}

			SampleIndex++;
		} // End Frame

		// Store Anim Info Data
		FAnim2TextureAnimInfo AnimInfo;
		AnimInfo.StartFrame = DataAsset->NumFrames;
		AnimInfo.EndFrame = DataAsset->NumFrames + AnimNumFrames - 1;
		DataAsset->Animations.Add(AnimInfo);

		// Accumulate Frames
		DataAsset->NumFrames += AnimNumFrames;

	} // End Anim
		
	// Destroy Temp Component & Actor
	SkeletalMeshComponent->UnregisterComponent();
	SkeletalMeshComponent->DestroyComponent();
	Actor->Destroy();
	
	// ---------------------------------------------------------------------------

	if (DataAsset->Mode == EAnim2TextureMode::Vertex)
	{
		// Find Best Resolution for Vertex Data
		int32 Height, Width;
		if (!FindBestResolution(DataAsset->NumFrames, NumVertices, 
								Height, Width, DataAsset->VertexRowsPerFrame, 
								DataAsset->MaxHeight, DataAsset->MaxWidth))
		{
			UE_LOG(LogVATInstancingEditor, Warning, TEXT("Vertex Animation data cannot be fit in a %ix%i texture."), DataAsset->MaxHeight, DataAsset->MaxWidth);
			return false;
		}

		// Normalize Vertex Data
		TArray<FVector3f> NormalizedVertexDeltas;
		TArray<FVector3f> NormalizedVertexNormals;
		NormalizeVertexData(
			VertexDeltas, VertexNormals,
			DataAsset->VertexMinBBox, DataAsset->VertexSizeBBox,
			NormalizedVertexDeltas, NormalizedVertexNormals);

		// Write Textures
		if (DataAsset->PositionPrecision == EAnim2TexturePrecision::SixteenBits)
		{
			WriteVectorsToTexture<FVector3f, FHighPrecision>(NormalizedVertexDeltas, DataAsset->NumFrames, DataAsset->VertexRowsPerFrame, Height, Width, DataAsset->GetVertexPositionTexture());
		}
		else
		{
			WriteVectorsToTexture<FVector3f, FLowPrecision>(NormalizedVertexDeltas, DataAsset->NumFrames, DataAsset->VertexRowsPerFrame, Height, Width, DataAsset->GetVertexPositionTexture());
		}

		if (DataAsset->RotationPrecision == EAnim2TexturePrecision::SixteenBits)
		{
			WriteVectorsToTexture<FVector3f, FHighPrecision>(NormalizedVertexNormals, DataAsset->NumFrames, DataAsset->VertexRowsPerFrame, Height, Width, DataAsset->GetVertexNormalTexture());
		}
		else
		{
			WriteVectorsToTexture<FVector3f, FLowPrecision>(NormalizedVertexNormals, DataAsset->NumFrames, DataAsset->VertexRowsPerFrame, Height, Width, DataAsset->GetVertexNormalTexture());
		}


		// Add Vertex UVChannel
		WriteVtxIdToNewUvChannel(DataAsset->GetStaticMesh(), DataAsset->StaticLODIndex, DataAsset->UVChannel, Height, Width);

		// Update Bounds
		SetBoundsExtensions(DataAsset->GetStaticMesh(), static_cast<FVector>(DataAsset->VertexMinBBox), static_cast<FVector>(DataAsset->VertexSizeBBox));

		// Done with StaticMesh
		DataAsset->GetStaticMesh()->PostEditChange();
	}

	// ---------------------------------------------------------------------------
	
	if (DataAsset->Mode == EAnim2TextureMode::Bone)
	{
		// Find Best Resolution for Bone Data
		int32 Height, Width;

		// Write Bone Position and Rotation Textures
		{
			// Note we are adding +1 frame for the ref pose
			if (!FindBestResolution(DataAsset->NumFrames + 1, DataAsset->NumBones,
				Height, Width, DataAsset->BoneRowsPerFrame,
				DataAsset->MaxHeight, DataAsset->MaxWidth))
			{
				UE_LOG(LogVATInstancingEditor, Warning, TEXT("Bone Animation data cannot be fit in a %ix%i texture."), DataAsset->MaxHeight, DataAsset->MaxWidth);
				return false;
			}

			// 把RefPose放在Bone Position Texture的最后一帧. RefPose Rotation在顶点着色器中其实用不到，单纯占位罢了
			// Note: Epic官方把refPose放到第零帧，导致将Frame归一化为SampleUV前要+1，并非最优
			BonePositions.Append(BoneRefPositions);
			BoneRotations.Append(BoneRefRotations_NoUse);

			// Normalize Bone Data
			TArray<FVector3f> NormalizedBonePositions;
			TArray<FVector4f> NormalizedBoneRotations;
			NormalizeBoneData(
				BonePositions, BoneRotations,
				DataAsset->BoneMinBBox, DataAsset->BoneSizeBBox,
				NormalizedBonePositions, NormalizedBoneRotations);

			// Write Textures
			if (DataAsset->PositionPrecision == EAnim2TexturePrecision::SixteenBits)
			{
				WriteVectorsToTexture<FVector3f, FHighPrecision>(NormalizedBonePositions, DataAsset->NumFrames + 1, DataAsset->BoneRowsPerFrame, Height, Width, DataAsset->GetBonePositionTexture());
			}
			else
			{
				WriteVectorsToTexture<FVector3f, FLowPrecision>(NormalizedBonePositions, DataAsset->NumFrames + 1, DataAsset->BoneRowsPerFrame, Height, Width, DataAsset->GetBonePositionTexture());
			}

			if (DataAsset->RotationPrecision == EAnim2TexturePrecision::SixteenBits)
			{
				WriteVectorsToTexture<FVector4f, FHighPrecision>(NormalizedBoneRotations, DataAsset->NumFrames + 1, DataAsset->BoneRowsPerFrame, Height, Width, DataAsset->GetBoneRotationTexture());
			}
			else
			{
				WriteVectorsToTexture<FVector4f, FLowPrecision>(NormalizedBoneRotations, DataAsset->NumFrames + 1, DataAsset->BoneRowsPerFrame, Height, Width, DataAsset->GetBoneRotationTexture());
			}

			// Update Bounds
			SetBoundsExtensions(DataAsset->GetStaticMesh(), static_cast<FVector>(DataAsset->BoneMinBBox), static_cast<FVector>(DataAsset->BoneSizeBBox));
		}

		// ---------------------------------------------------------------------------
		
		// Write Bone Influences
		{
			// Find Best Resolution for Bone Weights Texture
			if (!FindBestResolution(2, NumVertices,
				Height, Width, DataAsset->BoneWeightRowsPerFrame,
				DataAsset->MaxHeight, DataAsset->MaxWidth))
			{
				UE_LOG(LogVATInstancingEditor, Warning, TEXT("Weights Data cannot be fit in a %ix%i texture."), DataAsset->MaxHeight, DataAsset->MaxWidth);
				return false;
			}

			TArray<TVertexSkinWeight<4>> SkinWeights;

			// Reduce BoneWeights to 4 Influences.
			if (SocketIndex == INDEX_NONE)
			{
				// Project SkinWeights from SkeletalMesh to StaticMesh
				TArray<VertexSkinWeightMax> StaticMeshSkinWeights;
				Mapping.ProjectSkinWeights(StaticMeshSkinWeights);

				// Reduce Weights to 4 highest influences.
				ReduceSkinWeights(StaticMeshSkinWeights, SkinWeights);
			}
			// If Valid Socket, set all influences to same index.
			else
			{
				// Set all indices and weights to same SocketIndex
				SkinWeights.SetNumUninitialized(NumVertices);
				for (TVertexSkinWeight<4>& SkinWeight : SkinWeights)
				{
					SkinWeight.BoneWeights = TStaticArray<uint8, 4>(InPlace, 255);
					SkinWeight.MeshBoneIndices = TStaticArray<uint16, 4>(InPlace, SocketIndex);
				}
			}

			WriteSkinWeightsToColorAndBoneIdToUvChannel(SkinWeights, DataAsset);

		}

		// Done with StaticMesh
		DataAsset->GetStaticMesh()->PostEditChange();
	}

	DataAsset->MarkPackageDirty();
	return true;
}

int32 UVATInstancingBPLibrary::BatchUpdateAnimToTextureAssets(TArray<FString>& FailedAssets)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UMyAnimToTextureDataAsset::StaticClass()->GetClassPathName(), AssetDataList);

	int32 SuccessCount = 0;
	FScopedSlowTask SlowTask(AssetDataList.Num(), LOCTEXT("BatchUpdateAnimToTexture", "Batch Updating AnimToTexture Assets..."));
	SlowTask.MakeDialog();

	for (const FAssetData& AssetData : AssetDataList)
	{
		SlowTask.EnterProgressFrame(1, FText::FromString(FString::Printf(TEXT("Processing %s"), *AssetData.AssetName.ToString())));
		
		UMyAnimToTextureDataAsset* DataAsset = Cast<UMyAnimToTextureDataAsset>(AssetData.GetAsset());
		if (DataAsset)
		{
			if (AnimationToTexture(DataAsset))
			{
				SuccessCount++;
			}
			else
			{
				FailedAssets.Add(DataAsset->GetPathName());
			}
		}
		else
		{
			FailedAssets.Add(AssetData.GetObjectPathString());
		}
	}

	return SuccessCount;
}

void UVATInstancingBPLibrary::VatiDumpRegistryState()
{
	FOutputDevice& Ar = *GLog;
	Ar.Logf(TEXT("--- Dumping VATInstanceRegistry State ---"));

	const TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UObject>>& Renderers = VATInstanceRegistry::GetRenderers();

	Ar.Logf(TEXT("Total Tracked Worlds: %d"), Renderers.Num());

	for (const auto& Elem : Renderers)
	{
		const UWorld* World = Elem.Key.Get();
		UObject* RendererObject = Elem.Value.Get();

		if (World && RendererObject)
		{
			Ar.Logf(TEXT("- World: %s (Type: %d)"), *World->GetName(), (uint8)World->WorldType);
			IVATInstanceRendererInterface* Renderer = Cast<IVATInstanceRendererInterface>(RendererObject);
			if(Renderer)
			{
				Renderer->DumpDebugInfo(Ar);
			}
		}
	}
	Ar.Logf(TEXT("--- End of Dump ---"));
}


UPerInstanceCustomDataLayout* GetLayoutFromMaterialInterface(UMaterialInterface* MaterialInterface) 
{
	if (!MaterialInterface)
	{
		return nullptr;
	}

	if (auto Data = MaterialInterface->GetAssetUserData<UPerInstanceCustomDataLayoutUserData>(); Data && Data->Layout)
	{
		return Data->Layout;
	}

	if (const UMaterialInstance* InstanceMaterial = Cast<UMaterialInstance>(MaterialInterface))
	{
		return GetLayoutFromMaterialInterface(InstanceMaterial->Parent);
	}

	return nullptr;
}

void UVATInstancingBPLibrary::UpdateMaterialInstanceFromDataAsset(UMyAnimToTextureDataAsset* DataAsset, UMaterialInstanceConstant* MaterialInstance, 
	const EMaterialParameterAssociation MaterialParameterAssociation)
{
	check(DataAsset);
	check(MaterialInstance);


	if (!DataAsset->CustomDataLayout)
	{
		// If no CustomDataLayout is set, try to get it from the MaterialInstance's Asset User Data
		DataAsset->CustomDataLayout = GetLayoutFromMaterialInterface(MaterialInstance);
		if (DataAsset->CustomDataLayout)
		{
			for (const auto& entry : DataAsset->CustomDataLayout->ParameterNameToIndexMap)
			{
				DataAsset->NumCustomDataFloatsForVAT = FMath::Max(DataAsset->NumCustomDataFloatsForVAT, entry.Value + 1);
			}
		}

	}
	
	// Set UVChannel
	switch (DataAsset->UVChannel)
	{
		case 0:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, true, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, false, MaterialParameterAssociation);
			break;
		case 1:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, true, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, false, MaterialParameterAssociation);
			break;
		case 2:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, true, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, false, MaterialParameterAssociation);
			break;
		case 3:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, true, MaterialParameterAssociation);
			break;
		default:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, true, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, false, MaterialParameterAssociation);
			break;
	}

	// Update Vertex Params
	if (DataAsset->Mode == EAnim2TextureMode::Vertex)
	{
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MaterialInstance, AnimToTextureParamNames::MinBBox, FLinearColor(DataAsset->VertexMinBBox), MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MaterialInstance, AnimToTextureParamNames::SizeBBox, FLinearColor(DataAsset->VertexSizeBBox), MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::RowsPerFrame, DataAsset->VertexRowsPerFrame, MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, AnimToTextureParamNames::VertexPositionTexture, DataAsset->GetVertexPositionTexture(), MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, AnimToTextureParamNames::VertexNormalTexture, DataAsset->GetVertexNormalTexture(), MaterialParameterAssociation);
	}

	// Update Bone Params
	else if (DataAsset->Mode == EAnim2TextureMode::Bone)
	{
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::NumBones, DataAsset->NumBones, MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MaterialInstance, AnimToTextureParamNames::MinBBox, FLinearColor(DataAsset->BoneMinBBox), MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MaterialInstance, AnimToTextureParamNames::SizeBBox, FLinearColor(DataAsset->BoneSizeBBox), MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::RowsPerFrame, DataAsset->BoneRowsPerFrame, MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::BoneWeightRowsPerFrame, DataAsset->BoneWeightRowsPerFrame, MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, AnimToTextureParamNames::BonePositionTexture, DataAsset->GetBonePositionTexture(), MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, AnimToTextureParamNames::BoneRotationTexture, DataAsset->GetBoneRotationTexture(), MaterialParameterAssociation);

		// Num Influences
		switch (DataAsset->NumBoneInfluences)
		{
			case EAnim2TextureNumBoneInfluences::One:
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseTwoInfluences, false, MaterialParameterAssociation);
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseFourInfluences, false, MaterialParameterAssociation);
				break;
			case EAnim2TextureNumBoneInfluences::Two:
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseTwoInfluences, true, MaterialParameterAssociation);
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseFourInfluences, false, MaterialParameterAssociation);
				break;
			case EAnim2TextureNumBoneInfluences::Four:
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseTwoInfluences, false, MaterialParameterAssociation);
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseFourInfluences, true, MaterialParameterAssociation);
				break;
		}
	}

	
	// NumFrames
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::NumFrames, DataAsset->NumFrames, MaterialParameterAssociation);

	// SampleRate
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::SampleRate, DataAsset->SampleRate, MaterialParameterAssociation);

	// Update Material
	UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);

	// Rebuild Material
	UMaterialEditingLibrary::RebuildMaterialInstanceEditors(MaterialInstance->GetMaterial());

	// Set Preview Mesh
	if (DataAsset->GetStaticMesh())
	{
		MaterialInstance->PreviewMesh = DataAsset->GetStaticMesh();
	}

	MaterialInstance->MarkPackageDirty();
}


UStaticMesh* UVATInstancingBPLibrary::ConvertSkeletalMeshToStaticMesh(USkeletalMesh* SkeletalMesh, const FString PackageName, const int32 LODIndex)
{
	check(SkeletalMesh);

	if (PackageName.IsEmpty() || !FPackageName::IsValidObjectPath(PackageName))
	{
		return nullptr;
	}

	if (!SkeletalMesh->IsValidLODIndex(LODIndex))
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid LODIndex: %i"), LODIndex);
		return nullptr;
	}

	// Create Temp Actor
	check(GEditor);
	UWorld* World = GEditor->GetEditorWorldContext().World();
	check(World);
	AActor* Actor = World->SpawnActor<AActor>();
	check(Actor);

	// Create Temp SkeletalMesh Component
	USkeletalMeshComponent* MeshComponent = NewObject<USkeletalMeshComponent>(Actor);
	MeshComponent->RegisterComponent();
	MeshComponent->SetSkeletalMesh(SkeletalMesh);
	TArray<UMeshComponent*> MeshComponents = { MeshComponent };

	UStaticMesh* OutStaticMesh = nullptr;
	bool bGeneratedCorrectly = true;

	// Create New StaticMesh
	if (!FPackageName::DoesPackageExist(PackageName))
	{
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		OutStaticMesh = MeshUtilities.ConvertMeshesToStaticMesh(MeshComponents, FTransform::Identity, PackageName);
	}
	// Update Existing StaticMesh
	else
	{
		// Load Existing Mesh
		OutStaticMesh = LoadObject<UStaticMesh>(nullptr, *PackageName);
	}

	if (OutStaticMesh)
	{
		// Create Temp Package.
		// because 
		UPackage* TransientPackage = GetTransientPackage();

		// Create Temp Mesh.
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		UStaticMesh* TempMesh = MeshUtilities.ConvertMeshesToStaticMesh(MeshComponents, FTransform::Identity, TransientPackage->GetPathName());

		// make sure transactional flag is on
		TempMesh->SetFlags(RF_Transactional);

		// Copy All LODs
		if (LODIndex < 0)
		{
			const int32 NumSourceModels = TempMesh->GetNumSourceModels();
			OutStaticMesh->SetNumSourceModels(NumSourceModels);

			for (int32 Index = 0; Index < NumSourceModels; ++Index)
			{
				// Get RawMesh
				FRawMesh RawMesh;
				TempMesh->GetSourceModel(Index).LoadRawMesh(RawMesh);

				// Set RawMesh
				OutStaticMesh->GetSourceModel(Index).SaveRawMesh(RawMesh);
			};
		}

		// Copy Single LOD
		else
		{
			if (LODIndex >= TempMesh->GetNumSourceModels())
			{
				UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid Source Model Index: %i"), LODIndex);
				bGeneratedCorrectly = false;
			}
			else
			{
				OutStaticMesh->SetNumSourceModels(1);

				// Get RawMesh
				FRawMesh RawMesh;
				TempMesh->GetSourceModel(LODIndex).LoadRawMesh(RawMesh);

				// Set RawMesh
				OutStaticMesh->GetSourceModel(0).SaveRawMesh(RawMesh);
			}
		}
			
		// Copy Materials
		const TArray<FStaticMaterial>& Materials = TempMesh->GetStaticMaterials();
		OutStaticMesh->SetStaticMaterials(Materials);

		// Done
		TArray<FText> OutErrors;
		OutStaticMesh->Build(true, &OutErrors);

		FStaticMeshSourceModel& SourceModel = OutStaticMesh->GetSourceModel(LODIndex);
		SourceModel.BuildSettings.bGenerateLightmapUVs = false;

		OutStaticMesh->MarkPackageDirty();
	}

	// Destroy Temp Component and Actor
	MeshComponent->UnregisterComponent();
	MeshComponent->DestroyComponent();
	Actor->Destroy();

	return bGeneratedCorrectly ? OutStaticMesh : nullptr;
}


TArray<int32> GetWedgeUniqueIndexMap(FRawMesh& Mesh)
{
	TArray<int32> MeshWedgeUniqueIds;
	int32 Duplicates = 0;
	//UE_LOG(LogTemp, Display, TEXT("[UVertexAnimationBPLibrary::GetWedgeUniqueIndexMap] Wedge Position duplicate check, mesh vertex list: %d"), Mesh.VertexPositions.Num());
	for (int32 i = 0; i < Mesh.VertexPositions.Num(); i++)
	{
		const FVector3f& v = Mesh.VertexPositions[i];

		int32 VertId = Mesh.VertexPositions.Find(v);
		if (VertId != i)
		{
			//UE_LOG(LogTemp, Display, TEXT("[UVertexAnimationBPLibrary::GetWedgeUniqueIndexMap] Wedge Position duplicate: %d = %d"), i, VertId);
			Duplicates++;
		}

		MeshWedgeUniqueIds.Add(VertId);
	}

	//UE_LOG(LogTemp, Display, TEXT("[UVertexAnimationBPLibrary::GetWedgeUniqueIndexMap] Wedge Position duplicates: %d"), Duplicates);

	return MeshWedgeUniqueIds;
}

bool WriteSkinWeightsToColorAndBoneIdToUvChannel(TArray<TVertexSkinWeight<4>>& SkinWeights, UMyAnimToTextureDataAsset* DataAsset)
{
	UStaticMesh* StaticMesh = DataAsset->GetStaticMesh();
	const int32 LODIndex = DataAsset->StaticLODIndex;
	const int32 UVChannelIndex = DataAsset->UVChannel;
	float TextureSizeX = DataAsset->NumBones;

	TMap<FVertexInstanceID, FVector2D> TexCoordsBone12, TexCoordsBone34;

	// 曾尝试用下面api修改颜色的值，但是发现会自动做gamma校正，修改我的值。
	// FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LODIndex);
	// auto VertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color);

	FRawMesh Mesh;
	StaticMesh->GetSourceModel(LODIndex).LoadRawMesh(Mesh);
	// Map wedge vertex index to unique dataset
	TArray<int32> WedgeUniqueIndexMap = GetWedgeUniqueIndexMap(Mesh);
	// Reserve space for the new vertex colors.
	if (Mesh.WedgeColors.Num() == 0 || Mesh.WedgeColors.Num() != Mesh.WedgeIndices.Num())
	{
		Mesh.WedgeColors.Empty(Mesh.WedgeIndices.Num());
		Mesh.WedgeColors.AddUninitialized(Mesh.WedgeIndices.Num());
	}
	// Build a mapping of vertex positions to vertex colors.
	for (int32 WedgeIndex = 0; WedgeIndex < Mesh.WedgeIndices.Num(); ++WedgeIndex)
	{
		int32 VertID = Mesh.WedgeIndices[WedgeIndex];
		VertID = WedgeUniqueIndexMap[VertID];

		auto& w = SkinWeights[VertID].BoneWeights;
		auto& b = SkinWeights[VertID].MeshBoneIndices;

		Mesh.WedgeColors[WedgeIndex] = FColor(w[0], w[1], w[2], w[3]);

		// 在这里完成BoneId到SampleUV.x的转换, 以减少shader指令数
		TexCoordsBone12.Add(WedgeIndex, FVector2D(b[0] + 0.5f, b[1] + 0.5f) / TextureSizeX);
		TexCoordsBone34.Add(WedgeIndex, FVector2D(b[2] + 0.5f, b[3] + 0.5f) / TextureSizeX);
	}

	// Save the new raw mesh.
	StaticMesh->GetSourceModel(LODIndex).SaveRawMesh(Mesh);
	StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

	// 需要2个UVChannel，储存4个BoneId
	int32 NumUVChannels = StaticMesh->GetNumUVChannels(LODIndex);
	int32 NumUVChannelsToInsert = UVChannelIndex - NumUVChannels + 2;
	for (int32 Id = NumUVChannels; Id < NumUVChannels + NumUVChannelsToInsert; ++Id)
	{
		StaticMesh->InsertUVChannel(LODIndex, Id);
	}
	StaticMesh->SetUVChannel(LODIndex, UVChannelIndex, TexCoordsBone12);
	StaticMesh->SetUVChannel(LODIndex, UVChannelIndex + 1, TexCoordsBone34);
	SetFullPrecisionUVs(StaticMesh, LODIndex, true);

	return true;
}


#undef LOCTEXT_NAMESPACE