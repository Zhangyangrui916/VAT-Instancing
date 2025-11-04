#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include <HAL/Platform.h>
#include <UObject/ObjectMacros.h>
#include <UObject/ObjectPtr.h>
#include <Engine/EngineTypes.h>
#include "MyAnimToTextureDataAsset.generated.h"

class UAnimSequence;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

UENUM(Blueprintable)
enum class EAnim2TextureMode : uint8
{
	/* Position and Normal Per-Vertex */
	Vertex,
	/* Linear Blending Skinning */
	Bone,
};

UENUM(Blueprintable)
enum class EAnim2TexturePrecision : uint8
{
	/* 8 bits */
	EightBits,
	/* 16 bits */
	SixteenBits,
};

UENUM(Blueprintable)
enum class EAnim2TextureNumBoneInfluences : uint8
{
	/* Single bone influence */
	One,
	/* Blend between two influences */
	Two,
	/* Blend between four influences */
	Four,
};


USTRUCT(Blueprintable)
struct FVtxAnimComponentSpaceTransform
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Location;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FQuat Rotation;
};

USTRUCT(Blueprintable)
struct FAnim2TextureAnimSequenceInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite)
	TObjectPtr<UAnimSequence> AnimSequence = nullptr;

	/* Use Custom FrameRange */
	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite)
	bool bUseCustomRange = false;

	/* Animation Start Frame */
	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite, meta = (EditCondition = "bUseCustomRange", EditConditionHides))
	int32 StartFrame = 0;

	/* Animation End Frame (Inclusive) */
	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite, meta = (EditCondition = "bUseCustomRange", EditConditionHides))
	int32 EndFrame = 0;

	/* 预计算这些骨骼的ComponentSpaceTransform，运行时可以读取以获取Socket在动画某时刻的位置 */
	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite)
	TArray<FName> BoneOrSocketsOfInterest;

	/* 假设共N帧动画、M个骨骼，则对于第i帧、第j个骨骼的下标为(i*M+j) */
	UPROPERTY(VisibleAnywhere)
	TArray<FVtxAnimComponentSpaceTransform> BoneComponentSpaceTransforms;
};

USTRUCT(Blueprintable)
struct FAnim2TextureAnimInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Default, BlueprintReadOnly)
	int32 StartFrame = 0;

	// inclusive
	UPROPERTY(VisibleAnywhere, Category = Default, BlueprintReadOnly)
	int32 EndFrame = 0;
};


UCLASS(Blueprintable, BlueprintType)
class VATINSTANCING_API UMyAnimToTextureDataAsset : public UPrimaryDataAsset
{
public:
	GENERATED_BODY()

	/**
	 * The number of float values this visual type's VAT system will use from Per-Instance Custom Data.
	 * e.g., 1 for (CurrentFrame), 3 for (FrameA, FrameB, Alpha).
	 * The material for this visual type must be authored to expect this many floats starting at CustomDataStartOffset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VAT Instancing Config", meta = (ClampMin = "1", ClampMax = "16"))  // Max 16 is typical for custom data
	int32 NumCustomDataFloatsForVAT = 8;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VAT Instancing Config")
	TObjectPtr<class UPerInstanceCustomDataLayout> CustomDataLayout;

	/**
	* SkeletalMesh to bake animations from.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkeletalMesh", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/**
	* SkeletalMesh LOD to bake.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkeletalMesh", Meta = (DisplayName = "LODIndex"))
	int32 SkeletalLODIndex = 0;

	/**
	* StaticMesh to bake to.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMesh", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/**
	* StaticMesh LOD to bake to.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMesh", Meta = (DisplayName = "LODIndex"))
	int32 StaticLODIndex = 0;

	/**
	* StaticMesh UVChannel Index for storing vertex information.
	* Make sure this index does not conflict with the Lightmap UV Index.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMesh")
	int32 UVChannel = 1;

	/**
	* Number of Driver Triangles
	* Each StaticMesh Vertex will be influenced by N SkeletalMesh (Driver) Triangles.
	* Increasing the Number of Driver Triangles will increase the Mapping computation.
	* Using a single Driver Triangle will do a Rigid Binding to Closest Triangle.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMesh|Mapping")
	int32 NumDriverTriangles = 10;

	/**
	* Inverse Distance Weighting
	* This exponent value will be used for computing weights for the DriverTriangles.
	* Larger number will create a more contrasted weighting, but it might 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMesh|Mapping")
	float Sigma = 1.f;

	// ------------------------------------------------------
	// Texture

	/**
	* Max resolution of the texture.
	* A smaller size will be used if the data fits.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	int32 MaxHeight = 8192;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	int32 MaxWidth = 8192;

	/**
	* Texture Precision
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	EAnim2TexturePrecision PositionPrecision = EAnim2TexturePrecision::SixteenBits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	EAnim2TexturePrecision RotationPrecision = EAnim2TexturePrecision::SixteenBits;

	/**
	* Storage Mode.
	* Vertex: will store per-vertex position and normal.
	* Bone: Will store per-bone position and rotation and per-vertex bone weight. 
	        This is the preferred method if meshes share skeleton.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	EAnim2TextureMode Mode = EAnim2TextureMode::Bone;

	/**
	* Texture for storing vertex positions
	* This is only used on Vertex Mode
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture", meta = (EditCondition = "Mode == EAnim2TextureMode::Vertex", EditConditionHides))
	TSoftObjectPtr<UTexture2D> VertexPositionTexture;

	/**
	* Texture for storing vertex normals
	* This is only used on Vertex Mode
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture", meta = (EditCondition = "Mode == EAnim2TextureMode::Vertex", EditConditionHides))
	TSoftObjectPtr<UTexture2D> VertexNormalTexture;

	/**
	* Texture for storing bone positions
	* This is only used on Bone Mode
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture", meta = (EditCondition = "Mode == EAnim2TextureMode::Bone", EditConditionHides))
	TSoftObjectPtr<UTexture2D> BonePositionTexture;

	/**
	* Texture for storing bone rotations
	* This is only used on Bone Mode
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture", meta = (EditCondition = "Mode == EAnim2TextureMode::Bone", EditConditionHides))
	TSoftObjectPtr<UTexture2D> BoneRotationTexture;


	// ------------------------------------------------------
	// Animation

	/**
	* Adds transformation to baked textures. 
	* This can be used for adding an offset to the animation.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FTransform RootTransform;

	/** 
	* Bone used for Rigid Binding. The bone needs to be part of the RawBones. 
	* Sockets and VirtualBones are not supported.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (EditCondition = "Mode == EAnim2TextureMode::Bone"))
	FName AttachToSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float SampleRate = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TArray<FAnim2TextureAnimSequenceInfo> AnimSequences;

	/* 对所有动画预计算这些骨骼的ComponentSpaceTransform. 如果Socket只在某些动画时被用到，则应当为它们单独设置，以节约内存 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TArray<FName> BoneOrSocketsOfInterestForAllAnimSequences;


	/**
	* Number of Bone Influences for deformation. More influences will produce smoother results at the cost of performance.
	* This will be used by UpdateMaterialInstanceFromDataAsset and AssetActions for setting MaterialInstance static switches
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (EditCondition = "Mode == EAnim2TextureMode::Bone", EditConditionHides))
	EAnim2TextureNumBoneInfluences NumBoneInfluences = EAnim2TextureNumBoneInfluences::Four;

	// ------------------------------------------------------
	// Info

	/* Total Number of Frames in all animations */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo")
	int32 NumFrames = 0;

	/* Total Number of Bones */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo", Meta = (EditCondition = "Mode == EAnim2TextureMode::Bone", EditConditionHides))
	int32 NumBones = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo", Meta = (EditCondition = "Mode == EAnim2TextureMode::Vertex", EditConditionHides))
	int32 VertexRowsPerFrame = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo", Meta = (DisplayName = "MinBBox", EditCondition = "Mode == EAnim2TextureMode::Vertex", EditConditionHides))
	FVector3f VertexMinBBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo", Meta = (DisplayName = "SizeBBox", EditCondition = "Mode == EAnim2TextureMode::Vertex", EditConditionHides))
	FVector3f VertexSizeBBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo", Meta = (EditCondition = "Mode == EAnim2TextureMode::Bone", EditConditionHides))
	int32 BoneWeightRowsPerFrame = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo", Meta = (EditCondition = "Mode == EAnim2TextureMode::Bone", EditConditionHides))
	int32 BoneRowsPerFrame = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo", Meta = (DisplayName = "MinBBox", EditCondition = "Mode == EAnim2TextureMode::Bone", EditConditionHides))
	FVector3f BoneMinBBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo", Meta = (DisplayName = "SizeBBox", EditCondition = "Mode == EAnim2TextureMode::Bone", EditConditionHides))
	FVector3f BoneSizeBBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedInfo")
	TArray<FAnim2TextureAnimInfo> Animations;

	/* Finds AnimSequence Index in the Animations Array. 
	*  Only Enabled elements are returned.
	*  Returns -1 if not found.
	*/
	UFUNCTION(BlueprintCallable, Category = Default)
	int32 GetIndexFromAnimSequence(const UAnimSequence* Sequence);


	bool DoesSocketExist(FName InSocketName) const;

	void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const;

	bool GetSocketTransform(FName InSocketName, int32 AnimIndex, float AnimTime, FTransform& ComponentSpaceOut) const;

	UFUNCTION()
	void ResetInfo();

	
	UStaticMesh* GetStaticMesh() const;
	USkeletalMesh* GetSkeletalMesh() const;
	UTexture2D* GetVertexPositionTexture() const;
	UTexture2D* GetVertexNormalTexture() const;
	UTexture2D* GetBonePositionTexture() const;
	UTexture2D* GetBoneRotationTexture() const; 

	UFUNCTION(BlueprintPure, Category = Default, meta = (DisplayName = "Get Static Mesh"))
	UStaticMesh* BP_GetStaticMesh() { return GetStaticMesh(); }

	UFUNCTION(BlueprintPure, Category = Default, meta = (DisplayName = "Get Skeletal Mesh"))
	USkeletalMesh* BP_GetSkeletalMesh() { return GetSkeletalMesh(); }

	UFUNCTION(BlueprintPure, Category = Default, meta = (DisplayName = "Get Bone Position Texture"))
	UTexture2D* BP_GetBonePositionTexture() { return GetBonePositionTexture(); }

	UFUNCTION(BlueprintPure, Category = Default, meta = (DisplayName = "Get Bone Rotation Texture"))
	UTexture2D* BP_GetBoneRotationTexture() { return GetBoneRotationTexture(); }
};