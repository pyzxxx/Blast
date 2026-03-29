#include "Renderer.h"

void Renderer::Initialize()
{
    m_ctx = new RenderContext();
    m_scene = new RenderScene();
    m_opacityPass = new OpacityPass();
    m_compositePass = new CompositePass();
}

void Renderer::Terminate()
{
    if (m_swapchain)
    {
        RHI::DestroySwapchain(m_swapchain);
        m_swapchain = nullptr;
    }

    delete m_compositePass;
    delete m_opacityPass;
    delete m_scene;
    delete m_ctx;
}

void Renderer::SetWindow(void* nativeHandle)
{
    if (m_swapchain)
    {
        RHI::DestroySwapchain(m_swapchain);
        m_swapchain = nullptr;
    }

    RHI::CreateSwapchain(nativeHandle, m_swapchain);
    m_ctx->m_width = 0;
    m_ctx->m_height = 0;
}

void Renderer::AddExtension(RenderExtension* extension)
{
    if (extension)
    {
        m_extensions.push_back(extension);
    }
}

void Renderer::RemoveExtension(RenderExtension* extension)
{
    auto it = std::find(m_extensions.begin(), m_extensions.end(), extension);
    if (it != m_extensions.end())
        m_extensions.erase(it);
}

void Renderer::Render()
{
    if (!m_swapchain)
        return;

    RHI::SwapchainStatus swapchainStatus = RHI::UpdateSwapchain(m_swapchain);
    if (swapchainStatus == RHI::SwapchainStatus::NotReady)
        return;

    if (m_ctx->m_width != m_swapchain->width || m_ctx->m_height != m_swapchain->height)
    {
        m_ctx->m_width = m_swapchain->width;
        m_ctx->m_height = m_swapchain->height;
    }

    RHI::AcquireNextImage(m_swapchain);

    RHI::CommandBuffer* cmd = RHI::RequestCommandBuffer();

    Setup(cmd);
    Execute(cmd);

    RHI::Submit(cmd);
    RHI::Present(m_swapchain);
    RHI::NextFrame();

    DrawListRegister::ClearAll();
}

void Renderer::Setup(RHI::CommandBuffer* cmd)
{
    RHI::TextureDesc outputDesc = {};
    outputDesc.width = m_ctx->m_width;
    outputDesc.height = m_ctx->m_height;
    outputDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
    outputDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    m_ctx->CreateTexture("output", outputDesc, [&](RHI::Texture* texture) {
        RHI::CreateTextureView(texture, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

        RHI::ImageBarrier barrier = {};
        barrier.texture = texture;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        barrier.srcAccess = 0;
        barrier.dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);
    });

    m_scene->Setup(m_ctx, cmd);

    m_opacityPass->Setup(m_ctx, cmd);
    m_compositePass->Setup(m_ctx, cmd);

    for (auto* ext : m_extensions)
    {
        ext->Setup(m_ctx, cmd);
    }

    const RenderView& primaryView = m_scene->GetPrimaryView();

    PerFrameData perFrameData = {};
    perFrameData.view = primaryView.viewMatrix;
    perFrameData.projection = primaryView.projectionMatrix;
    perFrameData.viewProjection = primaryView.viewProjection;
    perFrameData.inverseView = primaryView.inverseView;
    perFrameData.inverseProjection = primaryView.inverseProjection;
    perFrameData.cameraPosition = glm::vec4(primaryView.cameraPosition, 1.0f);

    RHI::BufferDesc perFrameDesc = {};
    perFrameDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    perFrameDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    perFrameDesc.size = sizeof(PerFrameData);
    perFrameDesc.dynamicBuffer = true;
    RHI::Buffer* perFrameBuffer = m_ctx->CreateBuffer("perFrame", perFrameDesc);

    void* perFrameMap = RHI::MapMemory(perFrameBuffer);
    memcpy(perFrameMap, &perFrameData, sizeof(PerFrameData));
    RHI::UnmapMemory(perFrameBuffer);

    RHI::BufferBarrier perFrameBarrier = {};
    perFrameBarrier.buffer = perFrameBuffer;
    perFrameBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    perFrameBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    perFrameBarrier.dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    perFrameBarrier.dstAccess = VK_ACCESS_UNIFORM_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, perFrameBarrier);
}

void Renderer::Execute(RHI::CommandBuffer* cmd)
{
    RHI::Texture* output = m_ctx->GetTexture("output");

    m_opacityPass->Execute(m_ctx, cmd);
    m_compositePass->Execute(m_ctx, cmd);

    for (auto* ext : m_extensions)
    {
        ext->Execute(m_ctx, cmd);
    }

    RHI::ImageBarrier outputBarrier = {};
    outputBarrier.texture = output;
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    outputBarrier.srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    outputBarrier.srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    outputBarrier.dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    outputBarrier.dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, outputBarrier);

    RHI::ImageBarrier swapchainBarrier = {};
    swapchainBarrier.swapchain = m_swapchain;
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    swapchainBarrier.srcAccess = 0;
    swapchainBarrier.dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    swapchainBarrier.dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
    RHI::CmdPipelineBarrier(cmd, swapchainBarrier);

    RHI::CmdCopyTextureToSwapchain(cmd, output, m_swapchain);

    outputBarrier = {};
    outputBarrier.texture = output;
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    outputBarrier.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    outputBarrier.srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
    outputBarrier.dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    outputBarrier.dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    RHI::CmdPipelineBarrier(cmd, outputBarrier);

    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapchainBarrier.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    swapchainBarrier.srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapchainBarrier.dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    swapchainBarrier.dstAccess = 0;
    RHI::CmdPipelineBarrier(cmd, swapchainBarrier);
}