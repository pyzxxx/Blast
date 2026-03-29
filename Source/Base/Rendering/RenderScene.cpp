#include "RenderScene.h"
#include "Renderer.h"

RenderScene::RenderScene()
{
    m_nullView.viewMatrix = glm::mat4(1.0f);
    m_nullView.projectionMatrix = glm::mat4(1.0f);
    m_nullView.viewProjection = glm::mat4(1.0f);
    m_nullView.inverseView = glm::mat4(1.0f);
    m_nullView.inverseProjection = glm::mat4(1.0f);
    m_nullView.cameraPosition = glm::vec3(0.0f);
}

const RenderView& RenderScene::GetPrimaryView() const
{
    if (renderViews.Size() == 0)
    {
        return m_nullView;
    }
    
    const RenderView* view = renderViews.Get(m_primaryViewHandle);
    if (view)
    {
        return *view;
    }
    
    return m_nullView;
}

void RenderScene::SetPrimaryView(uint32_t renderViewHandle)
{
    if (renderViews.Get(renderViewHandle))
    {
        m_primaryViewHandle = renderViewHandle;
    }
}

void RenderScene::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::BufferDesc bufferDesc = {};
    bufferDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    bufferDesc.size = sizeof(GpuSceneNode) * gpuScene.Capacity();
    bufferDesc.dynamicBuffer = true;
    RHI::Buffer* gpuSceneBuffer = ctx->CreateBuffer("gpuScene", bufferDesc);

    uint8_t* mapData = (uint8_t*)RHI::MapMemory(gpuSceneBuffer);
    memcpy(mapData, gpuScene.Data(), sizeof(GpuSceneNode) * gpuScene.Size());
    RHI::UnmapMemory(gpuSceneBuffer);

    RHI::BufferBarrier bufferBrrier = {};
    bufferBrrier.buffer = gpuSceneBuffer;
    bufferBrrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    bufferBrrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    bufferBrrier.dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    bufferBrrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, bufferBrrier);
}
