#include "ImGuiRenderer.h"
#include "Foundation/FileSystem.h"
#include "Rendering/RenderContext.h"
#include "Rendering/ShaderCache.h"
#include "imgui.h"

ImGuiRenderer::ImGuiRenderer()
{
    ImGuiIO& io = ImGui::GetIO();

    int width, height;
    unsigned char* pixels = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    RHI::TextureDesc textureDesc = {};
    textureDesc.width = static_cast<uint32_t>(width);
    textureDesc.height = static_cast<uint32_t>(height);
    textureDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
    textureDesc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    RHI::CreateTexture(textureDesc, m_fontTexture);
    RHI::CreateBindless(m_fontTexture);

    RHI::EzSamplerDesc samplerDesc = {};
    samplerDesc.magFilter = VK_FILTER_LINEAR;
    samplerDesc.minFilter = VK_FILTER_LINEAR;
    samplerDesc.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    RHI::CreateSampler(samplerDesc, m_fontSampler);
    RHI::CreateBindless(m_fontSampler);

    io.Fonts->SetTexID((ImTextureID)(intptr_t)m_fontTexture);

    RHI::CommandBuffer* cmd = RHI::RequestCommandBuffer();

    RHI::ImageBarrier barrier = {};
    barrier.texture = m_fontTexture;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    barrier.srcAccess = 0;
    barrier.dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
    RHI::CmdPipelineBarrier(cmd, barrier);

    RHI::TextureRegion region = {};
    region.width = static_cast<uint32_t>(width);
    region.height = static_cast<uint32_t>(height);
    RHI::CmdUploadTexture(cmd, m_fontTexture, region, pixels);

    barrier.texture = m_fontTexture;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, barrier);

    RHI::Submit(cmd);
}

ImGuiRenderer::~ImGuiRenderer()
{
    if (m_fontSampler)
    {
        RHI::DestroySampler(m_fontSampler);
    }
    if (m_fontTexture)
    {
        RHI::DestroyTexture(m_fontTexture);
    }
}

void ImGuiRenderer::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::Texture* output = ctx->GetTexture("output");

    RHI::RenderPassDesc renderPassDesc = {};
    renderPassDesc.colors[0].texture = output;
    renderPassDesc.colors[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderPassDesc.colors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    RHI::RenderPass* renderPass = ctx->CreateRenderPass("imguiRenderPass", renderPassDesc);

    RHI::Shader* vertexShader = ShaderCache::Get()->GetShader(ShaderId::ImguiVert);
    RHI::Shader* fragmentShader = ShaderCache::Get()->GetShader(ShaderId::ImguiFrag);

    RHI::GraphicsPipelineDesc desc = {};
    desc.vertexShader = vertexShader;
    desc.fragmentShader = fragmentShader;
    desc.renderPass = renderPass;
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.cullMode = VK_CULL_MODE_NONE;

    desc.vertexLayout.attributes[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT};
    desc.vertexLayout.attributes[1] = {0, 8, VK_FORMAT_R32G32_SFLOAT};
    desc.vertexLayout.attributes[2] = {0, 16, VK_FORMAT_R8G8B8A8_UNORM};
    desc.vertexLayout.bindings[0] = {20, VK_VERTEX_INPUT_RATE_VERTEX};

    desc.blendState.blendEnable = true;
    desc.depthState.depthTest = false;
    desc.depthState.depthWrite = false;

    ctx->CreateGraphicsPipeline("imgui", desc);

    ImDrawData* drawData = ImGui::GetDrawData();
    m_drawCmdCount = 0;

    if (!drawData || drawData->TotalVtxCount == 0)
    {
        return;
    }

    uint32_t fbWidth = ctx->GetWidth();
    uint32_t fbHeight = ctx->GetHeight();

    if (fbWidth == 0 || fbHeight == 0)
    {
        return;
    }

    m_projMatrix = glm::ortho(0.0f, drawData->DisplaySize.x, 0.0f, drawData->DisplaySize.y, -1.0f, 1.0f);

    size_t vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    size_t indexSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

    RHI::BufferDesc vertexBufferDesc = {};
    vertexBufferDesc.size = vertexSize;
    vertexBufferDesc.dynamicBuffer = true;
    vertexBufferDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vertexBufferDesc.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    RHI::Buffer* vertexBuffer = ctx->CreateBuffer("imguiVertexBuffer", vertexBufferDesc);

    RHI::BufferDesc indexBufferDesc = {};
    indexBufferDesc.size = indexSize;
    indexBufferDesc.dynamicBuffer = true;
    indexBufferDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    indexBufferDesc.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    RHI::Buffer* indexBuffer = ctx->CreateBuffer("imguiIndexBuffer", indexBufferDesc);

    ImDrawVert* vtxDst = (ImDrawVert*)RHI::MapMemory(vertexBuffer);
    ImDrawIdx* idxDst = (ImDrawIdx*)RHI::MapMemory(indexBuffer);

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    RHI::UnmapMemory(vertexBuffer);
    RHI::UnmapMemory(indexBuffer);

    m_drawCmds.clear();
    int vtxOffset = 0;
    int idxOffset = 0;

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];

        for (int cmdIndex = 0; cmdIndex < cmdList->CmdBuffer.Size; cmdIndex++)
        {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIndex];

            if (!pcmd->UserCallback)
            {
                ImVec4 clipRect = pcmd->ClipRect;

                if (clipRect.x < fbWidth && clipRect.y < fbHeight && clipRect.z >= 0.0f && clipRect.w >= 0.0f)
                {
                    if (clipRect.x < 0.0f)
                    {
                        clipRect.x = 0.0f;
                    }
                    if (clipRect.y < 0.0f)
                    {
                        clipRect.y = 0.0f;
                    }

                    ImGuiDrawCmd drawCmd;
                    drawCmd.elemCount = pcmd->ElemCount;
                    drawCmd.idxOffset = pcmd->IdxOffset + idxOffset;
                    drawCmd.vtxOffset = pcmd->VtxOffset + vtxOffset;
                    drawCmd.clipRect[0] = clipRect.x;
                    drawCmd.clipRect[1] = clipRect.y;
                    drawCmd.clipRect[2] = clipRect.z;
                    drawCmd.clipRect[3] = clipRect.w;
                    m_drawCmds.push_back(drawCmd);
                }
            }
        }

        vtxOffset += cmdList->VtxBuffer.Size;
        idxOffset += cmdList->IdxBuffer.Size;
    }

    m_drawCmdCount = static_cast<int>(m_drawCmds.size());
}

void ImGuiRenderer::Execute(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    if (m_drawCmdCount == 0)
    {
        return;
    }

    RHI::Pipeline* pipeline = ctx->GetGraphicsPipeline("imgui");
    RHI::RenderPass* renderPass = ctx->GetRenderPass("imguiRenderPass");
    uint32_t fbWidth = ctx->GetWidth();
    uint32_t fbHeight = ctx->GetHeight();

    RHI::CmdBeginRenderPass(cmd, renderPass);
    RHI::CmdBindPipeline(cmd, pipeline);
    RHI::CmdSetViewport(cmd, 0, 0, (float)fbWidth, (float)fbHeight, 0.0f, 1.0f);
    RHI::CmdSetScissor(cmd, 0, 0, fbWidth, fbHeight);

    RHI::CmdBindTextureByName(cmd, "u_texture"_sh, m_fontTexture);
    RHI::CmdBindSamplerByName(cmd, "u_texture"_sh, m_fontSampler);

    RHI::Buffer* vertexBuffer = ctx->GetBuffer("imguiVertexBuffer");
    RHI::Buffer* indexBuffer = ctx->GetBuffer("imguiIndexBuffer");
    RHI::Buffer* buffers[] = {vertexBuffer};
    uint64_t offsets[] = {0};
    RHI::CmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    RHI::CmdBindIndexBuffer(cmd, indexBuffer, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

    RHI::CmdPushConstants(cmd, &m_projMatrix, sizeof(glm::mat4));

    for (int i = 0; i < m_drawCmdCount; i++)
    {
        const ImGuiDrawCmd& drawCmd = m_drawCmds[i];

        RHI::CmdSetScissor(cmd, (int32_t)drawCmd.clipRect[0], (int32_t)drawCmd.clipRect[1],
                           (int32_t)drawCmd.clipRect[2], (int32_t)drawCmd.clipRect[3]);

        RHI::CmdDrawIndexed(cmd, drawCmd.elemCount, drawCmd.idxOffset, drawCmd.vtxOffset);
    }

    RHI::CmdEndRenderPass(cmd);
}