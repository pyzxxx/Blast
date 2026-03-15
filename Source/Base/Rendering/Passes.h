#pragma once

#include "MeshRendering.h"

class BasePass
{
public:
    virtual void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) {}
    virtual void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) {}
};

class OpacityPass : public BasePass
{
public:
    OpacityPass();
    virtual ~OpacityPass();

    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
    void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
};