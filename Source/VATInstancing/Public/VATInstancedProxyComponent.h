#pragma once

#include "VatiDefines.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "CustomDataRecord.h"
#include "UObject/NameTypes.h"
#include "Animation/AnimTypes.h"
#include "VATInstancedProxyComponent.generated.h"

class UPerInstanceCustomDataLayout;
class UMaterialInterface;
class UMyAnimToTextureDataAsset;
struct FAnim2TextureAnimInfo;


DECLARE_STATS_GROUP(TEXT("VAT Instance Proxy"), STATGROUP_VATInstanceProxy, STATCAT_Advanced);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnimPlayToEnd, UVATInstancedProxyComponent*, ProxyComp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnimInterrupted, UVATInstancedProxyComponent*, ProxyComp);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VATINSTANCING_API UVATInstancedProxyComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UVATInstancedProxyComponent();

	/** Defines the visual type and available animations for this proxy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VAT Instancing")
	TObjectPtr<UMyAnimToTextureDataAsset> VisualTypeAsset;

	/** 默认为UMyAnimToTextureDataAsset中StaticMesh资产配置的材质，但是允许运行时设新的材质（切换到别的合批组） */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "VAT Instancing")
	TArray<TObjectPtr<UMaterialInterface>> CurrentBaseMaterials;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "VAT Instancing")
	TObjectPtr<UMaterialInterface> CurrentOverlayMaterial = nullptr;

	/**
	 * Plays a VAT animation by its index in the VisualTypeAsset's Animations array.
	 */
	UFUNCTION(BlueprintCallable, Category = "VAT Instancing")
	void PlayTexturedAnim(int32 AnimIndex, bool bShouldTransitionOnEnd = false, int32 NextAnimIndexOnEnd = -1, float BlendTime = 0.0f);
    
    UFUNCTION(BlueprintCallable, Category = "VAT Instancing")
	void PlayNamedTexturedAnim(FName AnimName, bool bShouldTransitionOnEnd = false, FName NextAnimNameOnEnd = NAME_None, float BlendTime = 0.0f);

	/** Stops the current animation. */
	UFUNCTION(BlueprintCallable, Category = "VAT Instancing")
	void StopTexturedAnim();

	/** Stops the current animation. */
	UFUNCTION(BlueprintCallable, Category = "VAT Instancing")
	FName GetPrimaryAnimName();

	UPROPERTY(BlueprintReadWrite)
	TArray<float> CurrentVATCustomData;  // Holds the per instance custom data

	/** If not defined, will get it from material's asset user data when begin play(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VAT Instancing")
	TObjectPtr<UPerInstanceCustomDataLayout> CustomDataLayout;

	UFUNCTION(BlueprintCallable, Category = "VAT Instancing")
	bool SetNamedCustomData(FName ParameterName, float Value);

	UFUNCTION(BlueprintCallable, Category = "VAT Instancing")
	int32 GetCustomDataIndexByName(FName ParameterName);

	// Helper to update CurrentVATCustomData based on animation state
	void PopulateVATCustomData();

	UFUNCTION(BlueprintCallable, Category = "VAT Instancing")
	void SetMaterialForSlot(int32 SlotIndex, UMaterialInterface* NewMaterial);

	// 设置一个空指针代表你希望该instance清除当前的覆盖材质
	UFUNCTION(BlueprintCallable, Category = "VAT Instancing")
	void SetOverlayMaterial(UMaterialInterface* NewOverlayMaterial, bool CreateDMIandOverwritePara = true);

	UFUNCTION(BlueprintCallable, Category = "VAT Instancing")
	void CommitMaterialChanges();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	struct AnimPlayState
	{
		int32 AnimIndex = -1;
		float AnimTime = 0.f;
		const FAnim2TextureAnimInfo* AnimInfo = nullptr;
	};
	
	AnimPlayState Primary, Secondary;

	UPROPERTY(BlueprintAssignable, Category = "VAT Instancing|Events")
	FOnAnimPlayToEnd OnAnimPlayToEnd;

	UPROPERTY(BlueprintAssignable, Category = "VAT Instancing|Events")
	FOnAnimInterrupted OnAnimInterrupted;

	bool bIsPlayingAnimation = false;

	bool bIsBlending = false;
	float CurrentBlendAlpha = 1.0;  // 0.0 = fully Secondary Anim, 1.0 = fully Primary Anim
	float TotalBlendDuration = 0.1;
	float CurrentBlendTimeElapsed;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VAT Instancing")
	float PlayRate = 1.f;


	int16 NextAnimIndexToPlayOnEnd;  //Index of animation to play when Primary Anim finishes
	bool bTransitionToNextOnEnd;     //Flag to enable this auto-transition

	void UpdateAnimation(float DeltaTime);
	void ProcessNotifiesForState(const AnimPlayState& State, float TickDeltaTime);

	UPROPERTY(transient)
	TArray<FAnimNotifyEvent> ActiveAnimNotifyStates;

	float CalculateAbsoluteFrame(const FAnim2TextureAnimInfo* AnimInfo, float AnimTime, float SampleRate);

	FORCEINLINE void UpdateRegistry() const;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	UCustomDataRecord* Record;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool ReadBackFromRecord = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FName ReadBackName;

	int32 RecordIndex = 0;  // Index in the record to read back from, if ReadBackFromRecord is true

	void ApplyReadBackData();

#endif
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
private:
	/** The unique identifier for this component instance within the VAT system. */
	FVATProxyId ProxyId;

	/** A static atomic counter to generate unique proxy IDs. */
	static TAtomic<uint64> NextProxyId;

	/** Helper function to register the component with the VAT system. */
	void RegisterWithVATSystem();

	/** Helper function to unregister the component from the VAT system. */
	void UnregisterFromVATSystem();

	bool bBatchKeyDirty = false;
};
