#pragma once

#include "MeshRendering.h"

class RenderContext;

class BasePass
{
public:
    virtual void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) {}
    virtual void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) {}
};

class LightClusterPass : public BasePass
{
public:
    LightClusterPass();
    virtual ~LightClusterPass();

    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
    void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) override;

private:
    void CreateClusterAABB(RenderContext* ctx, RHI::CommandBuffer* cmd);
    void AssignLightsToClusters(RenderContext* ctx, RHI::CommandBuffer* cmd);
};

class OpacityPass : public BasePass
{
public:
    OpacityPass();
    virtual ~OpacityPass();

    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
    void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) override;

private:
    void CreateOpaqueMeshPipeline(RenderContext* ctx);
    void CreateMaskMeshPipeline(RenderContext* ctx);
};

class AlphaPass : public BasePass
{
public:
    AlphaPass();
    virtual ~AlphaPass();

    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
    void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) override;

private:
    void CreateBlendMeshPipeline(RenderContext* ctx);
};

class CompositePass : public BasePass
{
public:
    CompositePass();
    virtual ~CompositePass();

    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
    void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
};

class BVHDebugPass : public BasePass
{
public:
    BVHDebugPass();
    virtual ~BVHDebugPass();

    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
    void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
};

