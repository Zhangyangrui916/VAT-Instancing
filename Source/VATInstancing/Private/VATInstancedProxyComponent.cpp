#include "VATInstancedProxyComponent.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifyQueue.h"
#include "GameFramework/Actor.h"
#include "Logging/LogMacros.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MyAnimToTextureDataAsset.h"
#include "PerInstanceCustomDataLayout.h"
#include "VATInstanceRegistry.h"
#include "VATMaterialParameterName.h"
#include "VertexAnimationNotifyInterface.h"
#include "VertexAnimationNotifyStateInterface.h"
#include "VisualLogger/VisualLogger.h"


DECLARE_CYCLE_STAT(TEXT("STAT_TickComponent"), STAT_VATInstanceProxyTick, STATGROUP_VATInstanceProxy);

// Initialize the static atomic counter for generating unique proxy IDs.
TAtomic<uint64> UVATInstancedProxyComponent::NextProxyId(1);

UVATInstancedProxyComponent::UVATInstancedProxyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	ProxyId = NextProxyId.AddExchange(1);
	bIsPlayingAnimation = false;
	bWantsInitializeComponent = true;
}

void UVATInstancedProxyComponent::OnRegister()
{
	Super::OnRegister();
	RegisterWithVATSystem();
}

void UVATInstancedProxyComponent::OnUnregister()
{
	UnregisterFromVATSystem();
	Super::OnUnregister();
}


void UVATInstancedProxyComponent::RegisterWithVATSystem()
{
#if WITH_EDITOR
	if (ProxyId == InvalidVATProxyId || !VisualTypeAsset)
	{
		ensure(false);
		return;
	}
#endif

	if (UStaticMesh* Mesh = VisualTypeAsset->GetStaticMesh())
	{
		const int32 NumSlots = Mesh->GetStaticMaterials().Num();
		CurrentBaseMaterials.SetNum(NumSlots);
		for (int32 i = 0; i < NumSlots; ++i)
		{
			CurrentBaseMaterials[i] = Mesh->GetMaterial(i);
		}
	}

	if (!CustomDataLayout)
	{
		CustomDataLayout = VisualTypeAsset->CustomDataLayout;
	}

	// Ensure the custom data array is the correct size.
	if (VisualTypeAsset->NumCustomDataFloatsForVAT > 0 && CurrentVATCustomData.Num() != VisualTypeAsset->NumCustomDataFloatsForVAT)
	{
		CurrentVATCustomData.SetNumZeroed(VisualTypeAsset->NumCustomDataFloatsForVAT);
	}
	
	// Populate initial state for CurrentVATCustomData
	PopulateVATCustomData();

	FBatchKey BatchKey(VisualTypeAsset, CurrentBaseMaterials, CurrentOverlayMaterial);
	VATInstanceRegistry::RegisterProxy(this, ProxyId, BatchKey, GetComponentTransform(), CurrentVATCustomData);
}

void UVATInstancedProxyComponent::UnregisterFromVATSystem()
{
	if (ProxyId != InvalidVATProxyId)
	{
		VATInstanceRegistry::UnregisterProxy(this, ProxyId);
	}
}

void UVATInstancedProxyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_VATInstanceProxyTick);

	if (bIsPlayingAnimation && Primary.AnimInfo && VisualTypeAsset)
	{
		UpdateAnimation(DeltaTime * PlayRate);
		PopulateVATCustomData();
		
		// Push the updated data to the renderer
		VATInstanceRegistry::NotifyProxyVisualsChanged(this, ProxyId, GetComponentTransform(), CurrentVATCustomData);
	}

#if WITH_EDITOR
	ApplyReadBackData();
#endif
}

void UVATInstancedProxyComponent::SetMaterialForSlot(int32 SlotIndex, UMaterialInterface* NewMaterial)
{
	if (!CurrentBaseMaterials.IsValidIndex(SlotIndex))
	{
		return;
	}

	if (CurrentBaseMaterials[SlotIndex] != NewMaterial)
	{
		CurrentBaseMaterials[SlotIndex] = NewMaterial;
		bBatchKeyDirty = true;
	}
}

void UVATInstancedProxyComponent::SetOverlayMaterial(UMaterialInterface* NewOverlayMaterial, bool CreateDMIandOverwritePara)
{
	if (NewOverlayMaterial == CurrentOverlayMaterial)
	{
		return;
	}

	if (CreateDMIandOverwritePara && NewOverlayMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(NewOverlayMaterial, VisualTypeAsset);

		FMaterialParameterInfo Para_BonePosition(AnimToTextureParamNames::BonePositionTexture, EMaterialParameterAssociation::LayerParameter, 0);
		FMaterialParameterInfo Para_BoneRotation(AnimToTextureParamNames::BoneRotationTexture, EMaterialParameterAssociation::LayerParameter, 0);
		MID->SetTextureParameterValueByInfo(Para_BonePosition, VisualTypeAsset->BonePositionTexture.Get());
		MID->SetTextureParameterValueByInfo(Para_BoneRotation, VisualTypeAsset->BoneRotationTexture.Get());

		FMaterialParameterInfo Para_MinBBox(AnimToTextureParamNames::MinBBox, EMaterialParameterAssociation::LayerParameter, 0);
		FMaterialParameterInfo Para_SizeBBox(AnimToTextureParamNames::SizeBBox, EMaterialParameterAssociation::LayerParameter, 0);
		MID->SetVectorParameterValueByInfo(Para_MinBBox, VisualTypeAsset->BoneMinBBox);
		MID->SetVectorParameterValueByInfo(Para_SizeBBox, VisualTypeAsset->BoneSizeBBox);

		CurrentOverlayMaterial = MID;
	}
	else
	{
		CurrentOverlayMaterial = NewOverlayMaterial;  // 如果不创建DMI，直接使用传入的材质
	}
	bBatchKeyDirty = true;
}

void UVATInstancedProxyComponent::CommitMaterialChanges()
{
	if (bBatchKeyDirty)
	{
		FBatchKey NewBatchKey(VisualTypeAsset, CurrentBaseMaterials, CurrentOverlayMaterial);
		VATInstanceRegistry::NotifyProxyBatchKeyChanged(this, ProxyId, NewBatchKey);
		bBatchKeyDirty = false;
	}
}

void UVATInstancedProxyComponent::PlayTexturedAnim(int32 NewAnimIndex, bool bShouldTransitionOnEnd, int32 NextAnimIndexOnEnd, float InBlendTime)
{
	if (OnAnimInterrupted.IsBound() && bIsPlayingAnimation && Primary.AnimIndex != NewAnimIndex)
	{
		OnAnimInterrupted.Broadcast(this);
	}

	/*
	 * 假设角色因为某些原因快速地进行了状态A->B->A的切换，那么称为BlendBack
	 * 这个时候，对alpha进行特殊处理，保证两个状态的混合权重随着时间的变化是连续的。
	 * 否则就简单的假设当前Primary动画已经完全混入，且将要开始混出。这未必正确，但是谁让BlendStack大小只有2呢。
	 */

	bIsBlending = Primary.AnimIndex != NewAnimIndex && InBlendTime > 0.0f;
	bool isBlendingBack = bIsBlending && (NewAnimIndex == Secondary.AnimIndex);
	float BlendBackTime = Secondary.AnimTime;
	
	Secondary = bIsBlending ? Primary : AnimPlayState();
	Primary.AnimIndex = NewAnimIndex;
	Primary.AnimInfo = &VisualTypeAsset->Animations[NewAnimIndex];
	Primary.AnimTime = isBlendingBack ? BlendBackTime : 0.f;
	CurrentBlendAlpha = bIsBlending ? (isBlendingBack ? 1 - CurrentBlendAlpha : 0.f) : 1.f;
	TotalBlendDuration = bIsBlending ? InBlendTime : 0.0f;
	CurrentBlendTimeElapsed = CurrentBlendAlpha * TotalBlendDuration;
	this->bTransitionToNextOnEnd = bShouldTransitionOnEnd;
	this->NextAnimIndexToPlayOnEnd = NextAnimIndexOnEnd;

	bIsPlayingAnimation = true;
	SetComponentTickEnabled(true);

	// Initial update
	PopulateVATCustomData();
	VATInstanceRegistry::NotifyProxyVisualsChanged(this, ProxyId, GetComponentTransform(), CurrentVATCustomData);
}

void UVATInstancedProxyComponent::StopTexturedAnim()
{
	bIsPlayingAnimation = false;
	SetComponentTickEnabled(false);
}

void UVATInstancedProxyComponent::PlayNamedTexturedAnim(FName AnimName, bool bShouldTransitionOnEnd, FName NextAnimNameOnEnd, float BlendTime)
{

	const int32 AnimIndex = VisualTypeAsset->AnimSequences.IndexOfByPredicate(
		[&](const FAnim2TextureAnimSequenceInfo& Info)
		{
			return Info.AnimSequence && Info.AnimSequence->GetFName() == AnimName;
		});

	if (AnimIndex == INDEX_NONE)
	{
		UE_LOG(LogVATInstancing, Warning, TEXT("PlayNamedTexturedAnim: Animation '%s' not found in VisualTypeAsset '%s'."), *AnimName.ToString(), *VisualTypeAsset->GetName());
		return;
	}

	int32 NextAnimIndex = INDEX_NONE;
	if (bShouldTransitionOnEnd)
	{
		NextAnimIndex = VisualTypeAsset->AnimSequences.IndexOfByPredicate(
			[&](const FAnim2TextureAnimSequenceInfo& Info)
			{
				return Info.AnimSequence && Info.AnimSequence->GetFName() == NextAnimNameOnEnd;
			});

		if (NextAnimIndex == INDEX_NONE)
		{
			UE_LOG(LogVATInstancing, Warning, TEXT("PlayNamedTexturedAnim: Next animation '%s' not found for transition."), *NextAnimNameOnEnd.ToString());
		}
	}
	
	PlayTexturedAnim(AnimIndex, bShouldTransitionOnEnd, NextAnimIndex, BlendTime);
}

FName UVATInstancedProxyComponent::GetPrimaryAnimName()
{
	if (VisualTypeAsset->Animations.IsValidIndex(Primary.AnimIndex))
	{
		if (const auto& AnimSeq = VisualTypeAsset->AnimSequences[Primary.AnimIndex].AnimSequence)
		{
			return AnimSeq->GetFName();
		}
	}
	return NAME_None;
}

void UVATInstancedProxyComponent::PopulateVATCustomData()
{
	if (!Primary.AnimInfo)
	{
		return;
	}

	// Calculate frame for CurrentPrimaryAnimInfo
	float FrameA = CalculateAbsoluteFrame(Primary.AnimInfo, Primary.AnimTime, VisualTypeAsset->SampleRate);

	if (bIsBlending && Secondary.AnimInfo)
	{
		float FrameB = CalculateAbsoluteFrame(Secondary.AnimInfo, Secondary.AnimTime, VisualTypeAsset->SampleRate);
		CurrentVATCustomData[0] = FrameA;
		CurrentVATCustomData[1] = FrameB;  // This should be the frame of the *previous* animation
		CurrentVATCustomData[2] = CurrentBlendAlpha;
	}
	else
	{
		CurrentVATCustomData[0] = FrameA;
		CurrentVATCustomData[1] = FrameA;
		CurrentVATCustomData[2] = 1.0f;
	}
}

void UVATInstancedProxyComponent::UpdateAnimation(float DeltaTime)
{
	if (!bIsPlayingAnimation || !Primary.AnimInfo)
	{
		return;
	}

	Primary.AnimTime += DeltaTime;
	if (bIsBlending)
	{
		Secondary.AnimTime += DeltaTime;
		CurrentBlendTimeElapsed += DeltaTime;
		CurrentBlendAlpha = FMath::Clamp(CurrentBlendTimeElapsed / TotalBlendDuration, 0.f, 1.f);
		if (CurrentBlendTimeElapsed >= TotalBlendDuration)
		{
			bIsBlending = false;
			Secondary = AnimPlayState(); // Clear secondary animation
		}
	}

	ProcessNotifiesForState(Primary, DeltaTime);

	// Note: NumOfInterval = NumOfFrame - 1
	const float SampleInterval = 1.f / VisualTypeAsset->SampleRate;
	const int32 PrimaryFrameNumInRange = (Primary.AnimInfo->EndFrame - Primary.AnimInfo->StartFrame + 1);
	const float PrimaryAnimDuration = (PrimaryFrameNumInRange - 1) * SampleInterval;
	
	const bool ShouldTransitionToNextAnim = (bTransitionToNextOnEnd && NextAnimIndexToPlayOnEnd != -1);

	// If we need to transition, start the blend "TotalBlendDuration" seconds before the end.
	const float TimeUntilEnd = PrimaryAnimDuration - Primary.AnimTime;
	const float TransitionThreshold = ShouldTransitionToNextAnim ? TotalBlendDuration : 0.0f;

	if (TimeUntilEnd <= TransitionThreshold)
	{
		if (OnAnimPlayToEnd.IsBound())
		{
			OnAnimPlayToEnd.Broadcast(this);
		}

		if (ShouldTransitionToNextAnim)
		{
			PlayTexturedAnim(NextAnimIndexToPlayOnEnd, bTransitionToNextOnEnd, NextAnimIndexToPlayOnEnd, TotalBlendDuration);
		}
		else
		{
			// Freeze at the last frame if not transitioning.
			Primary.AnimTime = PrimaryAnimDuration;
			bIsPlayingAnimation = false;
		}
	}
	else
	{
		// Ensure time does not exceed duration if we are not transitioning or looping.
		Primary.AnimTime = FMath::Clamp(Primary.AnimTime, 0.0f, PrimaryAnimDuration);
	}

	if (Secondary.AnimInfo)
	{
		const float SecondaryAnimDuration = (Secondary.AnimInfo->EndFrame - Secondary.AnimInfo->StartFrame) * SampleInterval;
		Secondary.AnimTime = FMath::Clamp(Secondary.AnimTime, 0.0f, SecondaryAnimDuration);
	}
}

float UVATInstancedProxyComponent::CalculateAbsoluteFrame(const FAnim2TextureAnimInfo* AnimInfo, float AnimTime, float SampleRate)
{
	if (!AnimInfo) return 0.0f;

	const float TotalAnimDurationInFrames = static_cast<float>(AnimInfo->EndFrame - AnimInfo->StartFrame + 1);
	const float StartFrameOfAnim = static_cast<float>(AnimInfo->StartFrame);  //+ 1.f;  // In texture, frame 0 is reserved for refpose
	float FrameInCurrentAnimSegment = AnimTime * SampleRate + 1e-2;                 //由于贴图采样用nearest，浮点误差容易导致采到上一帧

	float AbsoluteFrame = FMath::Clamp(StartFrameOfAnim + FrameInCurrentAnimSegment, StartFrameOfAnim, StartFrameOfAnim + TotalAnimDurationInFrames - 1e-2);

	// 将Frame转换为SampleUV.y
	AbsoluteFrame /= (1.0 + VisualTypeAsset->NumFrames);
	return AbsoluteFrame;
}


//See UAnimInstance::TriggerAnimNotifies
void UVATInstancedProxyComponent::ProcessNotifiesForState(const UVATInstancedProxyComponent::AnimPlayState& State, float TickDeltaTime)
{
	// Assumption: State.AnimIndex directly maps to VisualTypeAsset->AnimSequences index
	int32 OriginalAnimSequenceInfoIndex = State.AnimIndex;
	if (!VisualTypeAsset->AnimSequences.IsValidIndex(OriginalAnimSequenceInfoIndex))
		return;

	const FAnim2TextureAnimSequenceInfo& SeqInfo = VisualTypeAsset->AnimSequences[OriginalAnimSequenceInfoIndex];
	auto OriginalSequence = SeqInfo.AnimSequence.Get();

	if (!OriginalSequence)
		return;

	float OriginalSegmentStartTimeSecs = 0.f;
	if (SeqInfo.bUseCustomRange)
	{
		if (OriginalSequence->GetSamplingFrameRate().AsDecimal() == 0.f)
			return;  // Avoid division by zero
		OriginalSegmentStartTimeSecs = static_cast<float>(SeqInfo.StartFrame) / OriginalSequence->GetSamplingFrameRate().AsDecimal();
	}

	// Query interval is (PreviousTimeInOriginalSegment, CurrentTimeInOriginalSegment]   clamp？
	float PreviousTimeInOriginalSegment = OriginalSegmentStartTimeSecs + State.AnimTime - TickDeltaTime;
	float CurrentTimeInOriginalSegment = OriginalSegmentStartTimeSecs + State.AnimTime;

	if (CurrentTimeInOriginalSegment <= PreviousTimeInOriginalSegment)
		return;  // No forward progress or looped in a way not yet handled

	float QueryStartTime = PreviousTimeInOriginalSegment;
	float QueryDuration = CurrentTimeInOriginalSegment - PreviousTimeInOriginalSegment;

	FAnimNotifyContext NotifyContext;
	OriginalSequence->GetAnimNotifies(QueryStartTime, QueryDuration, NotifyContext);

	TArray<FAnimNotifyEvent> ActiveAnimNotifyStateThisFrame;
	TArray<const FAnimNotifyEvent*> AnimNotifyStateShouldFireBegin;

	// 通过对比这一帧与上一帧的AnimNotifyStates，来决定是否需要触发NotifyState的Begin/End事件
	for (const FAnimNotifyEventReference& EventReference : NotifyContext.ActiveNotifies)
	{
		const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify();
		if (!NotifyEvent)
			continue;

		const float NotifyStartTime = NotifyEvent->GetTriggerTime();
		const float NotifyEndTime = NotifyEvent->GetEndTriggerTime();

		if (NotifyEvent->Notify)
		{
			if (auto VANotify = Cast<IVertexAnimationNotifyInterface>(NotifyEvent->Notify))
			{
				VANotify->VertexAnimationNotify(this);
			}
			else if (NotifyEvent->Notify->GetClass()->ImplementsInterface(UVertexAnimationNotifyInterface::StaticClass()))
			{
				IVertexAnimationNotifyInterface::Execute_Received_VertexAnimationNotify(NotifyEvent->Notify, this);
			}
			else
			{
				UE_LOG(LogVATInstancing, Warning, TEXT("UVATInstancedProxyComponent on Actor %s: Notify %s does not implement IVertexAnimationNotifyInterface!"), *GetOwner()->GetName(), *NotifyEvent->Notify->GetName());
			}
		}

		if (NotifyEvent->NotifyStateClass)
		{
			if (auto VANotify = Cast<IVertexAnimationNotifyStateInterface>(NotifyEvent->NotifyStateClass) || NotifyEvent->NotifyStateClass->GetClass()->ImplementsInterface(UVertexAnimationNotifyStateInterface::StaticClass()))
			{
				int32 ExistingItemIndex = INDEX_NONE;
				if (ActiveAnimNotifyStates.Find(*NotifyEvent, ExistingItemIndex))
				{
					ActiveAnimNotifyStates.RemoveAtSwap(ExistingItemIndex, 1, false);
				}
				else
				{
					AnimNotifyStateShouldFireBegin.Add(NotifyEvent);
				}

				ActiveAnimNotifyStateThisFrame.Add(*NotifyEvent);
			}
		}
	}

	// Send End events for any active notify states that are not in the current frame's active notifies
	for (const FAnimNotifyEvent& ActiveNotify : ActiveAnimNotifyStates)
	{
		if (auto VAState = Cast<IVertexAnimationNotifyStateInterface>(ActiveNotify.NotifyStateClass))
		{
			VAState->VertexAnimationNotifyEnd(this);
		}
		else
		{
			IVertexAnimationNotifyStateInterface::Execute_Received_VertexAnimationNotifyEnd((UObject*)ActiveNotify.NotifyStateClass.Get(), this);
		}
	}

	// Send Begin events for any notify states that are in the current frame's active notifies but not in the previous frame's active notifies
	for (const FAnimNotifyEvent* ActiveNotify : AnimNotifyStateShouldFireBegin)
	{
		if (auto VAState = Cast<IVertexAnimationNotifyStateInterface>(ActiveNotify->NotifyStateClass))
		{
			VAState->VertexAnimationNotifyBegin(this, ActiveNotify->GetDuration());
		}
		else
		{
			IVertexAnimationNotifyStateInterface::Execute_Received_VertexAnimationNotifyBegin((UObject*)ActiveNotify->NotifyStateClass.Get(), this, ActiveNotify->GetDuration());
		}
	}

	// Update the active notify states to the current frame's active notifies
	ActiveAnimNotifyStates = MoveTemp(ActiveAnimNotifyStateThisFrame);
}

bool UVATInstancedProxyComponent::SetNamedCustomData(FName ParameterName, float Value)
{
#if !UE_BUILD_SHIPPING
	if (!CustomDataLayout)
	{
		UE_LOG(LogVATInstancing, Warning, TEXT("UVATInstancedProxyComponent on Actor %s: CustomDataLayout is not set. Cannot set named custom data."), *GetOwner()->GetName());
		return false;
	}
#endif

	int32* IndexPtr = CustomDataLayout->ParameterNameToIndexMap.Find(ParameterName);

#if !UE_BUILD_SHIPPING
	if (!IndexPtr)
	{
		UE_LOG(LogVATInstancing, Warning, TEXT("UVATInstancedProxyComponent on Actor %s: ParameterName %s not found in CustomDataLayout."), *GetOwner()->GetName(), *ParameterName.ToString());
		return false;
	}
	if (*IndexPtr < 0 || *IndexPtr >= CurrentVATCustomData.Num())
	{
		UE_LOG(LogVATInstancing, Warning, TEXT("UVATInstancedProxyComponent on Actor %s: Index %d for ParameterName %s is out of bounds."), *GetOwner()->GetName(), *IndexPtr, *ParameterName.ToString());
		return false;
	}
#endif

	CurrentVATCustomData[*IndexPtr] = Value;
	return true;
}

int32 UVATInstancedProxyComponent::GetCustomDataIndexByName(FName ParameterName)
{
#if !UE_BUILD_SHIPPING
	if (!CustomDataLayout)
	{
		UE_LOG(LogTemp, Warning, TEXT("UVATInstancedProxyComponent on Actor %s: CustomDataLayout is not set. Cannot set named custom data."), *GetOwner()->GetName());
		return false;
	}
#endif
	int32* IndexPtr = CustomDataLayout->ParameterNameToIndexMap.Find(ParameterName);
	return IndexPtr ? *IndexPtr : -1;
}


#if WITH_EDITOR
void UVATInstancedProxyComponent::ApplyReadBackData()
{
	if (ReadBackFromRecord)
	{
		while (ReadBackName != Record->Datas[RecordIndex].Name)
		{
			RecordIndex = (RecordIndex + 1) % Record->Datas.Num();
		}
		CurrentVATCustomData[0] = Record->Datas[RecordIndex].Values[0];
		CurrentVATCustomData[1] = Record->Datas[RecordIndex].Values[1];
		CurrentVATCustomData[2] = Record->Datas[RecordIndex].Values[2];
		this->SetComponentToWorld(Record->Datas[RecordIndex].Trans);
		++RecordIndex;
	}
}
#endif
