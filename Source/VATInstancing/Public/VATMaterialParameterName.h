#pragma once

#include <UObject/NameTypes.h>

namespace AnimToTextureParamNames
{
	//TODO: 使用这些参数实现autoPlay
	inline static const FName Frame = TEXT("Frame");
	inline static const FName AutoPlay = TEXT("AutoPlay");
	inline static const FName StartFrame = TEXT("StartFrame");
	inline static const FName EndFrame = TEXT("EndFrame");
	inline static const FName SampleRate = TEXT("SampleRate");
	inline static const FName NumFrames = TEXT("NumFrames");
	
	inline static const FName MinBBox = TEXT("MinBBox");
	inline static const FName SizeBBox = TEXT("SizeBBox");
	inline static const FName NumBones = TEXT("NumBones");
	inline static const FName RowsPerFrame = TEXT("RowsPerFrame");
	inline static const FName BoneWeightRowsPerFrame = TEXT("BoneWeightsRowsPerFrame");
	inline static const FName VertexPositionTexture = TEXT("PositionTexture");
	inline static const FName VertexNormalTexture = TEXT("NormalTexture");
	inline static const FName BonePositionTexture = TEXT("BonePositionTexture");
	inline static const FName BoneRotationTexture = TEXT("BoneRotationTexture");
	
	// 从勾选的通道开始，使用1个或2个通道来储存BoneInfluence信息。
	// 当UseBoneInfluenceTexture=true时，用1个通道储存BoneInfluenceTexture的采样UV。
	// 当UseBoneInfluenceTexture=false时，使用2个通道直接储存4个BoneId，BoneWeights在Color通道。
	inline static const FName UseUV0 = TEXT("UseUV0");
	inline static const FName UseUV1 = TEXT("UseUV1");
	inline static const FName UseUV2 = TEXT("UseUV2");
	inline static const FName UseUV3 = TEXT("UseUV3");
	
	inline static const FName UseTwoInfluences = TEXT("UseTwoInfluences");
	inline static const FName UseFourInfluences = TEXT("UseFourInfluences");
};  // namespace AnimToTextureParamNames
