#pragma once

#include "CoreMinimal.h"
#include "TickableEditorObject.h"

class FVatiDebugCollector : public FTickableEditorObject
{
public:
    FVatiDebugCollector();
    virtual ~FVatiDebugCollector();

    //~ Begin FTickableEditorObject Interface
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual TStatId GetStatId() const override;
    //~ End FTickableEditorObject Interface

private:
    double LastLogTime = 0.0;
};