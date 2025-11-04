#include "VertexAnimationNotifyStateInterface.h"


void IVertexAnimationNotifyStateInterface::VertexAnimationNotifyBegin(UVATInstancedProxyComponent* Proxy, float TotalDuration)
{
	Execute_Received_VertexAnimationNotifyBegin(Cast<UObject>(this), Proxy, TotalDuration);
}

void IVertexAnimationNotifyStateInterface::VertexAnimationNotifyTick(UVATInstancedProxyComponent* Proxy, float FrameDeltaTime)
{
	Execute_Received_VertexAnimationNotifyTick(Cast<UObject>(this), Proxy, FrameDeltaTime);
}

void IVertexAnimationNotifyStateInterface::VertexAnimationNotifyEnd(UVATInstancedProxyComponent* Proxy)
{
	Execute_Received_VertexAnimationNotifyEnd(Cast<UObject>(this), Proxy);
}

