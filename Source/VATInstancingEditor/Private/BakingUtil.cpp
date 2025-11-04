#pragma once

#include "BakingUtil.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "AnimToTextureSkeletalMesh.h"
#include "AnimToTextureUtils.h"
#include "AnimToTextureMeshMapping.h"
#include "MeshDescription.h"
#include "Math/Vector.h"
#include "VATInstancingEditorModule.h"
#include "MyAnimToTextureDataAsset.h"

using namespace AnimToTexture_Private;

bool FindBestResolution(const int32 NumFrames, const int32 NumElements, int32& OutHeight, int32& OutWidth, int32& OutRowsPerFrame, const int32 MaxHeight, const int32 MaxWidth)
{
	OutRowsPerFrame = FMath::CeilToInt(NumElements / (float)MaxWidth);
	OutWidth = FMath::CeilToInt(NumElements / (float)OutRowsPerFrame);
	OutHeight = NumFrames * OutRowsPerFrame;

	const bool bValidResolution = OutWidth <= MaxWidth && OutHeight <= MaxHeight;
	return bValidResolution;
}


void SetFullPrecisionUVs(UStaticMesh* StaticMesh, int32 LODIndex, bool bFullPrecision)
{
	check(StaticMesh);

	if (StaticMesh->IsSourceModelValid(LODIndex))
	{
		FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(LODIndex);
		SourceModel.BuildSettings.bUseFullPrecisionUVs = bFullPrecision;
	}
}


void SetBoundsExtensions(UStaticMesh* StaticMesh, const FVector& MinBBox, const FVector& SizeBBox)
{
	const FVector MaxBBox = SizeBBox + MinBBox;

	// Reset current extension bounds
	const FVector PositiveBoundsExtension = StaticMesh->GetPositiveBoundsExtension();
	const FVector NegativeBoundsExtension = StaticMesh->GetNegativeBoundsExtension();

	// Get current BoundingBox including extensions
	FBox BoundingBox = StaticMesh->GetBoundingBox();

	// Remove extensions from BoundingBox
	BoundingBox.Max -= PositiveBoundsExtension;
	BoundingBox.Min += NegativeBoundsExtension;

	// Calculate New BoundingBox
	FVector NewMaxBBox(FMath::Max(BoundingBox.Max.X, MaxBBox.X), FMath::Max(BoundingBox.Max.Y, MaxBBox.Y), FMath::Max(BoundingBox.Max.Z, MaxBBox.Z));

	FVector NewMinBBox(FMath::Min(BoundingBox.Min.X, MinBBox.X), FMath::Min(BoundingBox.Min.Y, MinBBox.Y), FMath::Min(BoundingBox.Min.Z, MinBBox.Z));

	// Calculate New Extensions
	FVector NewPositiveBoundsExtension = NewMaxBBox - BoundingBox.Max;
	FVector NewNegativeBoundsExtension = BoundingBox.Min - NewMinBBox;

	// Update StaticMesh
	StaticMesh->SetPositiveBoundsExtension(NewPositiveBoundsExtension);
	StaticMesh->SetNegativeBoundsExtension(NewNegativeBoundsExtension);
	StaticMesh->CalculateExtendedBounds();
}


void GetVertexDeltasAndNormals(const USkeletalMeshComponent* SkeletalMeshComponent,
														const int32 LODIndex,
														const AnimToTexture_Private::FSourceMeshToDriverMesh& SourceMeshToDriverMesh,
														const FTransform RootTransform,
														TArray<FVector3f>& OutVertexDeltas,
														TArray<FVector3f>& OutVertexNormals)
{
	OutVertexDeltas.Reset();
	OutVertexNormals.Reset();

	// Get Deformed vertices at current frame
	TArray<FVector3f> SkinnedVertices;
	GetSkinnedVertices(SkeletalMeshComponent, LODIndex, SkinnedVertices);

	// Get Source Vertices (StaticMesh)
	TArray<FVector3f> SourceVertices;
	const int32 NumVertices = SourceMeshToDriverMesh.GetSourceVertices(SourceVertices);

	// Deform Source Vertices with DriverMesh (SkeletalMesh
	TArray<FVector3f> DeformedVertices;
	TArray<FVector3f> DeformedNormals;
	SourceMeshToDriverMesh.DeformVerticesAndNormals(SkinnedVertices, DeformedVertices, DeformedNormals);

	// Allocate
	check(DeformedVertices.Num() == NumVertices && DeformedNormals.Num() == NumVertices);
	OutVertexDeltas.SetNumUninitialized(NumVertices);
	OutVertexNormals.SetNumUninitialized(NumVertices);

	// Transform Vertices and Normals with RootTransform
	for (int32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
	{
		const FVector3f& SourceVertex = SourceVertices[VertexIndex];
		const FVector3f& DeformedVertex = DeformedVertices[VertexIndex];
		const FVector3f& DeformedNormal = DeformedNormals[VertexIndex];

		// Transform Position and Delta with RootTransform
		const FVector3f TransformedVertexDelta = ((FVector3f)RootTransform.TransformPosition((FVector)DeformedVertex)) - SourceVertex;
		const FVector3f TransformedVertexNormal = (FVector3f)RootTransform.TransformVector((FVector)DeformedNormal);

		OutVertexDeltas[VertexIndex] = TransformedVertexDelta;
		OutVertexNormals[VertexIndex] = TransformedVertexNormal;
	}
}



int32 GetAnimationFrameRange(const FAnim2TextureAnimSequenceInfo& Animation, int32& OutStartFrame, int32& OutEndFrame)
{
	if (!Animation.AnimSequence)
	{
		return INDEX_NONE;
	}

	// Get Range from AnimSequence
	if (!Animation.bUseCustomRange)
	{
		OutStartFrame = 0;
		OutEndFrame = Animation.AnimSequence->GetNumberOfSampledKeys() - 1;  // AnimSequence->GetNumberOfFrames();
	}
	// Get Custom Range
	else
	{
		OutStartFrame = Animation.StartFrame;
		OutEndFrame = Animation.EndFrame;
	}

	// Return Number of Frames
	return OutEndFrame - OutStartFrame + 1;
}


void NormalizeVertexData(const TArray<FVector3f>& Deltas,
												  const TArray<FVector3f>& Normals,
												  FVector3f& OutMinBBox,
												  FVector3f& OutSizeBBox,
												  TArray<FVector3f>& OutNormalizedDeltas,
												  TArray<FVector3f>& OutNormalizedNormals)
{
	check(Deltas.Num() == Normals.Num());

	// ---------------------------------------------------------------------------
	// Compute Bounding Box
	//
	OutMinBBox = {TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max()};
	FVector3f MaxBBox = {TNumericLimits<float>::Min(), TNumericLimits<float>::Min(), TNumericLimits<float>::Min()};

	for (const FVector3f& Delta : Deltas)
	{
		// Find Min/Max BoundingBox
		OutMinBBox.X = FMath::Min(Delta.X, OutMinBBox.X);
		OutMinBBox.Y = FMath::Min(Delta.Y, OutMinBBox.Y);
		OutMinBBox.Z = FMath::Min(Delta.Z, OutMinBBox.Z);

		MaxBBox.X = FMath::Max(Delta.X, MaxBBox.X);
		MaxBBox.Y = FMath::Max(Delta.Y, MaxBBox.Y);
		MaxBBox.Z = FMath::Max(Delta.Z, MaxBBox.Z);
	}

	OutSizeBBox = MaxBBox - OutMinBBox;

	// ---------------------------------------------------------------------------
	// Normalize Vertex Position Deltas
	// Basically we want all deltas to be between [0, 1]

	// Compute Normalization Factor per-axis.
	const FVector3f NormFactor = {1.f / static_cast<float>(OutSizeBBox.X), 1.f / static_cast<float>(OutSizeBBox.Y), 1.f / static_cast<float>(OutSizeBBox.Z)};

	OutNormalizedDeltas.SetNumUninitialized(Deltas.Num());
	for (int32 Index = 0; Index < Deltas.Num(); ++Index)
	{
		OutNormalizedDeltas[Index] = (Deltas[Index] - OutMinBBox) * NormFactor;
	}

	// ---------------------------------------------------------------------------
	// Normalize Vertex Normals
	// And move them to [0, 1]

	OutNormalizedNormals.SetNumUninitialized(Normals.Num());
	for (int32 Index = 0; Index < Normals.Num(); ++Index)
	{
		OutNormalizedNormals[Index] = (Normals[Index].GetSafeNormal() + FVector3f::OneVector) * 0.5f;
	}
}




void NormalizeBoneData(const TArray<FVector3f>& Positions,
												const TArray<FVector4f>& Rotations,
												FVector3f& OutMinBBox,
												FVector3f& OutSizeBBox,
												TArray<FVector3f>& OutNormalizedPositions,
												TArray<FVector4f>& OutNormalizedRotations)
{
	check(Positions.Num() == Rotations.Num());

	// ---------------------------------------------------------------------------
	// Compute Position Bounding Box
	//
	OutMinBBox = {TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max()};
	FVector3f MaxBBox = {TNumericLimits<float>::Min(), TNumericLimits<float>::Min(), TNumericLimits<float>::Min()};

	for (const FVector3f& Position : Positions)
	{
		// Find Min/Max BoundingBox
		OutMinBBox.X = FMath::Min(Position.X, OutMinBBox.X);
		OutMinBBox.Y = FMath::Min(Position.Y, OutMinBBox.Y);
		OutMinBBox.Z = FMath::Min(Position.Z, OutMinBBox.Z);

		MaxBBox.X = FMath::Max(Position.X, MaxBBox.X);
		MaxBBox.Y = FMath::Max(Position.Y, MaxBBox.Y);
		MaxBBox.Z = FMath::Max(Position.Z, MaxBBox.Z);
	}

	OutSizeBBox = MaxBBox - OutMinBBox;

	// ---------------------------------------------------------------------------
	// Normalize Bone Position.
	// Basically we want all positions to be between [0, 1]

	// Compute Normalization Factor per-axis.
	const FVector3f NormFactor = {1.f / static_cast<float>(OutSizeBBox.X), 1.f / static_cast<float>(OutSizeBBox.Y), 1.f / static_cast<float>(OutSizeBBox.Z)};

	OutNormalizedPositions.SetNumUninitialized(Positions.Num());
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		OutNormalizedPositions[Index] = (Positions[Index] - OutMinBBox) * NormFactor;
	}

	// ---------------------------------------------------------------------------
	// Normalize Rotations
	// And move them to [0, 1]
	OutNormalizedRotations.SetNumUninitialized(Rotations.Num());
	constexpr float inv2Pi = 1.0 / (PI * 2.f);
	for (int32 Index = 0; Index < Rotations.Num(); ++Index)
	{
		const FVector4f Axis = Rotations[Index];
		const float Angle = Rotations[Index].W;  // Angle are returned in radians and they go from [0-pi*2]

		OutNormalizedRotations[Index] = (Axis.GetSafeNormal() + FVector3f::OneVector) * 0.5f;
		OutNormalizedRotations[Index].W = Angle * inv2Pi;
	}
}


bool CheckDataAsset(const UMyAnimToTextureDataAsset* DataAsset, int32& OutSocketIndex)
{
	// Check StaticMesh
	if (!DataAsset->GetStaticMesh())
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid StaticMesh"));
		return false;
	}

	// Check SkeletalMesh
	if (!DataAsset->GetSkeletalMesh())
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid SkeletalMesh"));
		return false;
	}

	// Check Skeleton
	if (!DataAsset->GetSkeletalMesh()->GetSkeleton())
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid SkeletalMesh. No valid Skeleton found"));
		return false;
	}

	// Check StaticMesh LOD
	if (!DataAsset->GetStaticMesh()->IsSourceModelValid(DataAsset->StaticLODIndex))
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid StaticMesh LOD Index: %i"), DataAsset->StaticLODIndex);
		return false;
	}

	// Check SkeletalMesh LOD
	if (!DataAsset->GetSkeletalMesh()->IsValidLODIndex(DataAsset->SkeletalLODIndex))
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid SkeletalMesh LOD Index: %i"), DataAsset->SkeletalLODIndex);
		return false;
	}

	// Check Socket.
	OutSocketIndex = INDEX_NONE;
	if (DataAsset->AttachToSocket.IsValid() && !DataAsset->AttachToSocket.IsNone())
	{
		// Get Bone Names (no virtual)
		TArray<FName> BoneNames;
		AnimToTexture_Private::GetBoneNames(DataAsset->GetSkeletalMesh(), BoneNames);

		// Check if Socket is in BoneNames
		if (!BoneNames.Find(DataAsset->AttachToSocket, OutSocketIndex))
		{
			UE_LOG(LogVATInstancingEditor, Warning, TEXT("Socket: %s not found in Raw Bone List"), *DataAsset->AttachToSocket.ToString());
			return false;
		}
		else
		{
			// TODO: SocketIndex can only be < TNumericLimits<uint16>::Max()
		}
	}

	// 我修改了ConvertSkeletalMeshToStaticMesh，使得其默认不生成lightmap。 跳过这个检查?
	const FStaticMeshSourceModel& SourceModel = DataAsset->GetStaticMesh()->GetSourceModel(DataAsset->StaticLODIndex);
	if (SourceModel.BuildSettings.bGenerateLightmapUVs && SourceModel.BuildSettings.DstLightmapIndex == DataAsset->UVChannel)
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid StaticMesh UVChannel: %i. Already used by LightMap"), DataAsset->UVChannel);
		return false;
	}

	// TODO: Maybe more bones is OK?
	const int32 NumBones = AnimToTexture_Private::GetNumBones(DataAsset->GetSkeletalMesh());
	if (NumBones > 256)
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("Too many Bones: %i. There is a maximum of 256 bones"), NumBones);
		return false;
	}

	for (const FAnim2TextureAnimSequenceInfo& AnimSequenceInfo : DataAsset->AnimSequences)
	{
		if (const UAnimSequence* AnimSequence = AnimSequenceInfo.AnimSequence)
		{
			// Make sure SkeletalMesh is compatible with AnimSequence
			if (!DataAsset->GetSkeletalMesh()->GetSkeleton()->IsCompatibleForEditor(AnimSequence->GetSkeleton()))
			{
				UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid AnimSequence: %s for given SkeletalMesh: %s"), *AnimSequence->GetFName().ToString(), *DataAsset->GetSkeletalMesh()->GetFName().ToString());
				return false;
			}

			// Check Frame Range
			if (AnimSequenceInfo.bUseCustomRange && (AnimSequenceInfo.StartFrame < 0 || AnimSequenceInfo.EndFrame > AnimSequence->GetNumberOfSampledKeys() - 1 || AnimSequenceInfo.EndFrame - AnimSequenceInfo.StartFrame < 0))
			{
				UE_LOG(LogVATInstancingEditor, Warning, TEXT("Invalid CustomRange for AnimSequence: %s"), *AnimSequence->GetName());
				return false;
			}
		}
	}

	if (!DataAsset->AnimSequences.Num())
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("No valid AnimSequences found"));
		return false;
	}

	return true;
}


int32 GetRefBonePositionsAndRotations(const USkeletalMesh* SkeletalMesh, TArray<FVector3f>& OutBoneRefPositions, TArray<FVector4f>& OutBoneRefRotations)
{
	check(SkeletalMesh);

	OutBoneRefPositions.Reset();
	OutBoneRefRotations.Reset();

	// Get Number of RawBones (no virtual)
	const int32 NumBones = AnimToTexture_Private::GetNumBones(SkeletalMesh);

	// Get Raw Ref Bone (no virtual)
	TArray<FTransform> RefBoneTransforms;
	AnimToTexture_Private::GetRefBoneTransforms(SkeletalMesh, RefBoneTransforms);
	AnimToTexture_Private::DecomposeTransformations(RefBoneTransforms, OutBoneRefPositions, OutBoneRefRotations);

	return NumBones;
}


int32 GetBonePositionsAndRotations(const USkeletalMeshComponent* SkeletalMeshComponent,
															const TArray<FVector3f>& BoneRefPositions,
															TArray<FVector3f>& BonePositions,
															TArray<FVector4f>& BoneRotations,
															int32 SampleIndex,
															FAnim2TextureAnimSequenceInfo& SeqInfo,
								   int32 Offset,
								   TMap<int32, int32> BoneId2InterestListIdThisAnim,
								   TMap<FName, int32> SocketName2InterestListIdThisAnim)
{
	check(SkeletalMeshComponent);

	BonePositions.Reset();
	BoneRotations.Reset();

	// Get Relative Transforms
	// Note: Size is of Raw bones in SkeletalMesh. These are the original/raw bones of the asset, without Virtual Bones.
	TArray<FMatrix44f> RefToLocals;
	SkeletalMeshComponent->CacheRefToLocalMatrices(RefToLocals);
	const int32 NumBones = RefToLocals.Num();

	// check size
	check(NumBones == BoneRefPositions.Num());

	// Get Component Space Transforms
	// Note returns all transforms, including VirtualBones
	const TArray<FTransform>& CompSpaceTransforms = SkeletalMeshComponent->GetComponentSpaceTransforms();
	check(CompSpaceTransforms.Num() >= RefToLocals.Num());

	BonePositions.SetNumUninitialized(NumBones);
	BoneRotations.SetNumUninitialized(NumBones);

	const int32 TotalNum = SeqInfo.BoneOrSocketsOfInterest.Num() + Offset;
	const int32 FrameOffset = SampleIndex * TotalNum;
	for (auto [SocketName, InterestListId] : SocketName2InterestListIdThisAnim)
	{
		FTransform SocketTrans = SkeletalMeshComponent->GetSocketTransform(SocketName, RTS_Component);

		SeqInfo.BoneComponentSpaceTransforms[FrameOffset + InterestListId].Location = SocketTrans.GetLocation();
		SeqInfo.BoneComponentSpaceTransforms[FrameOffset + InterestListId].Rotation = SocketTrans.GetRotation();
	}

	for (int32 BoneIndex = 0; BoneIndex < NumBones; BoneIndex++)
	{
		// Decompose Transformation (ComponentSpace)
		const FTransform& CompSpaceTransform = CompSpaceTransforms[BoneIndex];

		if (auto InterestListId = BoneId2InterestListIdThisAnim.Find(BoneIndex))
		{
			SeqInfo.BoneComponentSpaceTransforms[FrameOffset + *InterestListId].Location = CompSpaceTransform.GetLocation();
			SeqInfo.BoneComponentSpaceTransforms[FrameOffset + *InterestListId].Rotation = CompSpaceTransform.GetRotation();
		}

		FVector3f BonePosition;
		FVector4f BoneRotation;
		DecomposeTransformation(CompSpaceTransform, BonePosition, BoneRotation);

		// Position Delta (from RefPose)
		const FVector3f Delta = BonePosition - BoneRefPositions[BoneIndex];

		// Decompose Transformation (Relative to RefPose)
		FVector3f BoneRelativePosition;
		FVector4f BoneRelativeRotation;
		const FMatrix RefToLocalMatrix(RefToLocals[BoneIndex]);
		const FTransform RelativeTransform(RefToLocalMatrix);
		DecomposeTransformation(RelativeTransform, BoneRelativePosition, BoneRelativeRotation);

		BonePositions[BoneIndex] = Delta;
		BoneRotations[BoneIndex] = BoneRelativeRotation;
	}

	return NumBones;
}


bool WriteVtxIdToNewUvChannel(UStaticMesh* StaticMesh, const int32 LODIndex, const int32 UVChannelIndex, const int32 Height, const int32 Width)
{
	check(StaticMesh);

	if (!StaticMesh->IsSourceModelValid(LODIndex))
	{
		return false;
	}

	// ----------------------------------------------------------------------------
	// Get Mesh Description.
	// This is needed for Inserting UVChannel
	FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LODIndex);
	check(MeshDescription);

	// Add New UVChannel.
	if (UVChannelIndex == StaticMesh->GetNumUVChannels(LODIndex))
	{
		if (!StaticMesh->InsertUVChannel(LODIndex, UVChannelIndex))
		{
			UE_LOG(LogVATInstancingEditor, Warning, TEXT("Unable to Add UVChannel"));
			return false;
		}
	}
	else if (UVChannelIndex > StaticMesh->GetNumUVChannels(LODIndex))
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("UVChannel: %i Out of Range. Number of existing UVChannels: %i"), UVChannelIndex, StaticMesh->GetNumUVChannels(LODIndex));
		return false;
	}

	// -----------------------------------------------------------------------------

	TMap<FVertexInstanceID, FVector2D> TexCoords;

	for (const FVertexInstanceID VertexInstanceID : MeshDescription->VertexInstances().GetElementIDs())
	{
		const FVertexID VertexID = MeshDescription->GetVertexInstanceVertex(VertexInstanceID);
		const int32 VertexIndex = VertexID.GetValue();

		float U = (0.5f / (float)Width) + (VertexIndex % Width) / (float)Width;
		float V = (0.5f / (float)Height) + (VertexIndex / Width) / (float)Height;

		TexCoords.Add(VertexInstanceID, FVector2D(U, V));
	}

	// Set Full Precision UVs
	SetFullPrecisionUVs(StaticMesh, LODIndex, true);

	// Set UVs
	if (StaticMesh->SetUVChannel(LODIndex, UVChannelIndex, TexCoords))
	{
		return true;
	}
	else
	{
		UE_LOG(LogVATInstancingEditor, Warning, TEXT("Unable to Set UVChannel: %i. TexCoords: %i"), UVChannelIndex, TexCoords.Num());
		return false;
	};
}



