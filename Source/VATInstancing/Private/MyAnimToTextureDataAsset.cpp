#include "MyAnimToTextureDataAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

int32 UMyAnimToTextureDataAsset::GetIndexFromAnimSequence(const UAnimSequence* Sequence)
{
	const int32 NumSequences = AnimSequences.Num();

	// We can store a sequence to index map for a faster search
	for (int32 Index = 0; Index < NumSequences; ++Index)
	{
		const FAnim2TextureAnimSequenceInfo& SequenceInfo = AnimSequences[Index];
		if (SequenceInfo.AnimSequence == Sequence)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool UMyAnimToTextureDataAsset::DoesSocketExist(FName InSocketName) const
{
#if !UE_BUILD_SHIPPING
	if (AnimSequences.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMyAnimToTextureDataAsset on %s: Animations array is empty. Cannot check for socket %s."), *GetName(), *InSocketName.ToString());
		return false;
	}
#endif

	const auto Ske = GetSkeletalMesh() ? GetSkeletalMesh()->GetSkeleton() : nullptr;
	if (!Ske)
	{
		return false;
	}

	for (const auto& Socket: Ske->Sockets)
	{
		if (Socket->SocketName == InSocketName)
		{
			return true;
		}
	}

	for (auto& Bone : Ske->GetReferenceSkeleton().GetRefBoneInfo())
	{
		if (Bone.Name == InSocketName)
		{
			return true;
		}
	}
	return false;
}

void UMyAnimToTextureDataAsset::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	TSet<FName> NameSet;
	for (auto& Anim : AnimSequences)
	{
		for (FName Name : Anim.BoneOrSocketsOfInterest)
		{
			if (Name != NAME_None)
			{
				NameSet.Add(Name);;
			}
		}
	}

	const auto Ske = GetSkeletalMesh() ? GetSkeletalMesh()->GetSkeleton() : nullptr;
	if (!Ske)
	{
		return;
	}

	for (const FName& Name : NameSet)
	{
		bool IsSocket = (nullptr != Ske->FindSocket(Name)) ;
		OutSockets.Add(FComponentSocketDescription(Name, IsSocket ? EComponentSocketType::Socket : EComponentSocketType::Bone));
	}

}

bool UMyAnimToTextureDataAsset::GetSocketTransform(FName InSocketName, int32 AnimIndex, float AnimTime, FTransform& Out) const
{
	check(AnimSequences.IsValidIndex(AnimIndex));

	int32 j = BoneOrSocketsOfInterestForAllAnimSequences.Find(InSocketName);

	if (INDEX_NONE == j)
	{
		j = AnimSequences[AnimIndex].BoneOrSocketsOfInterest.Find(InSocketName);
		if (INDEX_NONE == j)
		{
			UE_LOG(LogTemp, Warning, TEXT("UMyAnimToTextureDataAsset on %s: Socket %s not found in AnimSequence %s."), *GetName(), *InSocketName.ToString(), *AnimSequences[AnimIndex].AnimSequence->GetName());
			return false;
		}
		j += BoneOrSocketsOfInterestForAllAnimSequences.Num();
	}

	auto& CachedTransform = AnimSequences[AnimIndex].BoneComponentSpaceTransforms;
	const int32 AnimLength = Animations[AnimIndex].EndFrame - Animations[AnimIndex].StartFrame;
	const int32 FrameIndex = FMath::Clamp(FMath::RoundHalfToZero(AnimTime * SampleRate), 0, AnimLength);

	const int32 TotalNum = AnimSequences[AnimIndex].BoneOrSocketsOfInterest.Num() + BoneOrSocketsOfInterestForAllAnimSequences.Num();
	const int32 FrameOffset = FrameIndex * TotalNum;


	const auto& [Location, Rotation] = CachedTransform[FrameOffset + j];
	Out.SetLocation(Location);
	Out.SetRotation(Rotation);
	return true;
}

void UMyAnimToTextureDataAsset::ResetInfo()
{
	// Common Info.
	NumFrames = 0;
	Animations.Reset();

	// Vertex Info
	VertexRowsPerFrame = 1;
	VertexMinBBox = FVector3f::ZeroVector;
	VertexSizeBBox = FVector3f::ZeroVector;

	// Bone Info
	NumBones = 0;
	BoneRowsPerFrame = 1;
	BoneWeightRowsPerFrame = 1;
	BoneMinBBox = FVector3f::ZeroVector;
	BoneSizeBBox = FVector3f::ZeroVector;

	// Cached Anim Transform
	for (FAnim2TextureAnimSequenceInfo& AnimSequence : AnimSequences)
	{
		AnimSequence.BoneComponentSpaceTransforms.Reset();
	}
};

// If we weren't in a plugin, we could unify this in a base class
template<typename AssetType>
static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer)
{
	AssetType* ReturnVal = nullptr;
	if (AssetPointer.ToSoftObjectPath().IsValid())
	{
		ReturnVal = AssetPointer.Get();
		if (!ReturnVal)
		{
			AssetType* LoadedAsset = Cast<AssetType>(AssetPointer.ToSoftObjectPath().TryLoad());
			if (ensureMsgf(LoadedAsset, TEXT("Failed to load asset pointer %s"), *AssetPointer.ToString()))
			{
				ReturnVal = LoadedAsset;
			}
		}
	}
	return ReturnVal;
}

UStaticMesh* UMyAnimToTextureDataAsset::GetStaticMesh() const
{
	return GetAsset(StaticMesh);
}

USkeletalMesh* UMyAnimToTextureDataAsset::GetSkeletalMesh() const
{
	return GetAsset(SkeletalMesh);
}

UTexture2D* UMyAnimToTextureDataAsset::GetVertexPositionTexture() const
{
	return GetAsset(VertexPositionTexture);
}

UTexture2D* UMyAnimToTextureDataAsset::GetVertexNormalTexture() const
{
	return GetAsset(VertexNormalTexture);
}

UTexture2D* UMyAnimToTextureDataAsset::GetBonePositionTexture() const
{
	return GetAsset(BonePositionTexture);
}

UTexture2D* UMyAnimToTextureDataAsset::GetBoneRotationTexture() const
{
	return GetAsset(BoneRotationTexture);
}
