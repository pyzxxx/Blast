#include "RenderScene.h"

void RenderScene::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    // Gpu Scene
    RHI::BufferDesc bufferDesc = {};
    bufferDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
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
    bufferBrrier.dstAccess = VK_ACCESS_UNIFORM_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, bufferBrrier);
}