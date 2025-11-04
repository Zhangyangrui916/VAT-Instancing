#include "VertexAnimationNotifyInterface.h"


void IVertexAnimationNotifyInterface::VertexAnimationNotify(UVATInstancedProxyComponent* Proxy)
{
	Execute_Received_VertexAnimationNotify(Cast<UObject>(this), Proxy);
}
