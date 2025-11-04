#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VertexAnimSkeleton.generated.h"


/* 挂载在对应的Actor上，并设置由哪个VATInstancedProxyComponent来驱动这个骨骼
 * GetSocketTransform是Lazy的，只要没有挂载物体，且没人询问，理论上没有开销。
 * 只有当主动调用时，才会根据VATInstancedProxyComponent的状态来计算对应骨骼的Transform。
 * (TODO:注意GetSocketTransform在ProxyComponent::UpdateAnimation的前还是后，是否会造成一帧的delay？)
 */

class UVATInstancedProxyComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VATINSTANCING_API UVertexAnimSkeleton : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVertexAnimSkeleton();

	UFUNCTION(BlueprintCallable)
	void SetInstanceProxy(UVATInstancedProxyComponent* proxy);

	UPROPERTY(VisibleInstanceOnly,Transient)
	TWeakObjectPtr<UVATInstancedProxyComponent> InstanceProxy;  // The proxy component that drives this skeleton

	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override final;
	virtual FVector GetSocketLocation(FName InSocketName) const override final;
	virtual FRotator GetSocketRotation(FName InSocketName) const override final;
	virtual FQuat GetSocketQuaternion(FName InSocketName) const override final;



	virtual bool HasAnySockets() const override;
	virtual bool DoesSocketExist(FName InSocketName) const override;
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
		
};
