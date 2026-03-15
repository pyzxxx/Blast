#pragma once

#include "RHI/RHI.h"

class RenderContext;

class RenderExtension
{
public:
    virtual ~RenderExtension() = default;
    virtual void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) = 0;
    virtual void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) = 0;
};
