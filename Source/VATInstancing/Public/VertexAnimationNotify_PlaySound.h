#pragma once

#include "VertexAnimationNotifyInterface.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "VertexAnimationNotify_PlaySound.generated.h"

UCLASS(const, hidecategories = Object, collapsecategories, Config = Game, meta = (DisplayName = "Vertex Animation Play Sound"))
class VATINSTANCING_API UVertexAnimationNotify_PlaySound
	: public UAnimNotify_PlaySound
	, public IVertexAnimationNotifyInterface
{
	GENERATED_BODY()


public:

	virtual void VertexAnimationNotify(UVATInstancedProxyComponent* Proxy) override final;
};
