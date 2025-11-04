#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VertexAnimationNotifyInterface.generated.h"

class UVATInstancedProxyComponent;


UINTERFACE(MinimalAPI, BlueprintType)
class UVertexAnimationNotifyInterface : public UInterface
{
	GENERATED_BODY()
};


class VATINSTANCING_API IVertexAnimationNotifyInterface
{
	GENERATED_BODY()

		// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintImplementableEvent)
	bool Received_VertexAnimationNotify(UVATInstancedProxyComponent* Proxy) const;

	virtual void VertexAnimationNotify(UVATInstancedProxyComponent* Proxy);

};
