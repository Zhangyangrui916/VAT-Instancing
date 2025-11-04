#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VertexAnimationNotifyStateInterface.generated.h"

class UVATInstancedProxyComponent;


UINTERFACE(MinimalAPI) 
class UVertexAnimationNotifyStateInterface : public UInterface
{
	GENERATED_BODY()
};


class VATINSTANCING_API IVertexAnimationNotifyStateInterface
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintImplementableEvent)
	bool Received_VertexAnimationNotifyBegin(UVATInstancedProxyComponent* Proxy, float TotalDuration) const;

	UFUNCTION(BlueprintImplementableEvent)
	bool Received_VertexAnimationNotifyTick(UVATInstancedProxyComponent* Proxy, float FrameDeltaTime) const;

	UFUNCTION(BlueprintImplementableEvent)
	bool Received_VertexAnimationNotifyEnd(UVATInstancedProxyComponent* Proxy) const;

	virtual void VertexAnimationNotifyBegin(UVATInstancedProxyComponent* Proxy, float TotalDuration);
	virtual void VertexAnimationNotifyTick(UVATInstancedProxyComponent* Proxy, float FrameDeltaTime);
	virtual void VertexAnimationNotifyEnd(UVATInstancedProxyComponent* Proxy);
};
