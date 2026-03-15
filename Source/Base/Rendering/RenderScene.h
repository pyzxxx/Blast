#pragma once

#include "Foundation/ObjectPool.h"
#include "Math/MathCommon.h"
#include "RHI/RHI.h"
#include "RenderContext.h"

struct GpuSceneNode
{
    glm::mat4 transform;
    glm::mat4 pad0;
    glm::mat4 pad1;
    glm::mat4 pad2;
};

class RenderScene
{
public:
    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd);

public:
    ObjectPool<GpuSceneNode> gpuScene;
};