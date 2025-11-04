#include "VertexAnimSkeleton.h"

#include "MyAnimToTextureDataAsset.h"
#include "VATInstancedProxyComponent.h"
#include "Engine/SkeletalMeshSocket.h"

// Sets default values for this component's properties
UVertexAnimSkeleton::UVertexAnimSkeleton()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVertexAnimSkeleton::SetInstanceProxy(UVATInstancedProxyComponent* proxy)
{
	InstanceProxy = proxy;
}

FTransform UVertexAnimSkeleton::GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace) const
{
	if (InSocketName == NAME_None || !InstanceProxy.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("VertexAnimSkeleton::GetSocketTransform failed. InSocketName is None or InstanceProxy is null."));
		return FTransform::Identity;
	}

	int32 PrimaryAnimIndex = InstanceProxy->Primary.AnimIndex;
	float PrimaryAnimTime = InstanceProxy->Primary.AnimTime;

	FTransform SocketComponentSpaceTrans = FTransform::Identity;
	if (!InstanceProxy->VisualTypeAsset->GetSocketTransform(InSocketName, PrimaryAnimIndex, PrimaryAnimTime, SocketComponentSpaceTrans))
	{
		UE_LOG(LogTemp, Log, TEXT("VertexAnimSkeleton::GetSocketTransform failed. "));
		return FTransform::Identity;
	}

	switch (TransformSpace)
	{
	case ERelativeTransformSpace::RTS_World:
	{
		SocketComponentSpaceTrans = SocketComponentSpaceTrans * InstanceProxy->GetComponentTransform();
	}
	case RTS_ParentBoneSpace:
		break;//不支持层级结构。 假设每个Socket都是独立的。
	case ERelativeTransformSpace::RTS_Actor:
		break;
	case RTS_Component:
		break;
	default:
		break;
	}

	
	return SocketComponentSpaceTrans;
}

FVector UVertexAnimSkeleton::GetSocketLocation(FName InSocketName) const
{
	return GetSocketTransform(InSocketName, ERelativeTransformSpace::RTS_World).GetTranslation();
}

FRotator UVertexAnimSkeleton::GetSocketRotation(FName InSocketName) const
{
	return GetSocketTransform(InSocketName, ERelativeTransformSpace::RTS_World).GetRotation().Rotator();
}

FQuat UVertexAnimSkeleton::GetSocketQuaternion(FName InSocketName) const
{
	return GetSocketTransform(InSocketName, ERelativeTransformSpace::RTS_World).GetRotation();
}

bool UVertexAnimSkeleton::HasAnySockets() const
{
	if (!InstanceProxy.IsValid())
	{
		return false;
	}
	return true;
}

bool UVertexAnimSkeleton::DoesSocketExist(FName InSocketName) const
{
	if (!InstanceProxy.IsValid() || !InstanceProxy->VisualTypeAsset)
	{
		return false;
	}
	return InstanceProxy->VisualTypeAsset->DoesSocketExist(InSocketName);
}

void UVertexAnimSkeleton::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	if (!InstanceProxy.IsValid() || !InstanceProxy->VisualTypeAsset)
	{
		return;
	}

	InstanceProxy->VisualTypeAsset->QuerySupportedSockets(OutSockets);
}


// Called when the game starts
void UVertexAnimSkeleton::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UVertexAnimSkeleton::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	//Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateChildTransforms(EUpdateTransformFlags::OnlyUpdateIfUsingSocket);
}


