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

struct RenderView
{
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 viewProjection;
    glm::mat4 inverseView;
    glm::mat4 inverseProjection;
    glm::vec3 cameraPosition;
};

class RenderScene
{
public:
    RenderScene();
    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd);

    const RenderView& GetPrimaryView() const;
    void SetPrimaryView(uint32_t renderViewHandle);

public:
    ObjectPool<GpuSceneNode> gpuScene;
    ObjectPool<RenderView> renderViews;

private:
    RenderView m_nullView;
    uint32_t m_primaryViewHandle = 0;
};
