#pragma once

#include "CoreMinimal.h"

namespace AnimToTexture_Private
{
	class FSourceMeshToDriverMesh;
}

struct FAnim2TextureAnimSequenceInfo;
class UMyAnimToTextureDataAsset;

bool FindBestResolution(const int32 NumFrames, const int32 NumElements, int32& OutHeight, int32& OutWidth, int32& OutRowsPerFrame, const int32 MaxHeight, const int32 MaxWidth);



// Returns Start, EndFrame and NumFrames in Animation
int32 GetAnimationFrameRange(const FAnim2TextureAnimSequenceInfo& Animation, int32& OutStartFrame, int32& OutEndFrame);

// Get Vertex and Normals from Current Pose
// The VertexDelta is returned from the RefPose
void GetVertexDeltasAndNormals(const USkeletalMeshComponent* SkeletalMeshComponent,
									  const int32 LODIndex,
									  const AnimToTexture_Private::FSourceMeshToDriverMesh& SourceMeshToDriverMesh,
									  const FTransform RootTransform,
									  TArray<FVector3f>& OutVertexDeltas,
									  TArray<FVector3f>& OutVertexNormals);

void SetFullPrecisionUVs(UStaticMesh* StaticMesh, const int32 LODIndex, bool bFullPrecision = true);

void SetBoundsExtensions(UStaticMesh* StaticMesh, const FVector& MinBBox, const FVector& SizeBBox);

// Normalizes Deltas and Normals between [0-1] with Bounding Box
void NormalizeVertexData(const TArray<FVector3f>& Deltas, const TArray<FVector3f>& Normals, FVector3f& OutMinBBox, FVector3f& OutSizeBBox, TArray<FVector3f>& OutNormalizedDeltas, TArray<FVector3f>& OutNormalizedNormals);

// Normalizes Positions and Rotations between [0-1] with Bounding Box
void NormalizeBoneData(const TArray<FVector3f>& Positions, const TArray<FVector4f>& Rotations, FVector3f& OutMinBBox, FVector3f& OutSizeBBox, TArray<FVector3f>& OutNormalizedPositions, TArray<FVector4f>& OutNormalizedRotations);

// Runs some validations for the assets in DataAsset
// Returns false if there is any problems with the data, warnings will be printed in Log
bool CheckDataAsset(const UMyAnimToTextureDataAsset* DataAsset, int32& OutSocketIndex);

	// Gets RefPose Bone Position and Rotations.
int32 GetRefBonePositionsAndRotations(const USkeletalMesh* SkeletalMesh, TArray<FVector3f>& OutBoneRefPositions, TArray<FVector4f>& OutBoneRefRotations);

	// Gets Bone Position and Rotations for Current Pose.
// The BonePosition is returned relative to the RefPose
int32 GetBonePositionsAndRotations(const USkeletalMeshComponent* SkeletalMeshComponent,
										  const TArray<FVector3f>& BoneRefPositions,
										  TArray<FVector3f>& BonePositions,
										  TArray<FVector4f>& BoneRotations,
										  int32 SampleIndex,
										  FAnim2TextureAnimSequenceInfo& SeqInfo,
										  int32 Offset,
										  TMap<int32, int32> BoneId2InterestListIdThisAnim,
										  TMap<FName, int32> SocketName2InterestListIdThisAnim);


/* 利用UV channel储存VertexId->TextureSampleUV的映射关系 */
bool WriteVtxIdToNewUvChannel(UStaticMesh* StaticMesh, const int32 LODIndex, const int32 UVChannelIndex, const int32 Height, const int32 Width);
