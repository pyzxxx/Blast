#include "Passes.h"
#include "RenderContext.h"
#include "ShaderCache.h"

OpacityPass::OpacityPass()
{
}

OpacityPass::~OpacityPass()
{
}

void OpacityPass::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    uint32_t width = ctx->GetWidth();
    uint32_t height = ctx->GetHeight();

    RHI::TextureDesc colorDesc = {};
    colorDesc.width = width;
    colorDesc.height = height;
    colorDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ctx->CreateTexture("opacityColor", colorDesc, [&](RHI::Texture* tex) {
        RHI::CreateTextureView(tex, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
        RHI::ImageBarrier barrier = {};
        barrier.texture = tex;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        barrier.srcAccess = 0;
        barrier.dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);
    });

    RHI::TextureDesc depthDesc = {};
    depthDesc.width = width;
    depthDesc.height = height;
    depthDesc.format = VK_FORMAT_D32_SFLOAT;
    depthDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ctx->CreateTexture("opacityDepth", depthDesc, [&](RHI::Texture* tex) {
        RHI::CreateTextureView(tex, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1);
        RHI::ImageBarrier barrier = {};
        barrier.texture = tex;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        barrier.srcAccess = 0;
        barrier.dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        barrier.dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);
    });

    RHI::RenderPassDesc renderPassDesc = {};
    renderPassDesc.colors[0].texture = ctx->GetTexture("opacityColor");
    renderPassDesc.colors[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    renderPassDesc.colors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassDesc.colors[0].clearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
    renderPassDesc.depth.texture = ctx->GetTexture("opacityDepth");
    renderPassDesc.depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    renderPassDesc.depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    renderPassDesc.depth.clearValue.depthStencil = {1.0f, 0};
    ctx->CreateRenderPass("opacity", renderPassDesc);

    RHI::GraphicsPipelineDesc pipelineDesc = {};
    pipelineDesc.vertexShader = ShaderCache::Get()->GetShader("Assets/Shader/Model.vert");
    pipelineDesc.fragmentShader = ShaderCache::Get()->GetShader("Assets/Shader/Model.frag");
    pipelineDesc.renderPass = ctx->GetRenderPass("opacity");
    pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineDesc.fillMode = VK_POLYGON_MODE_FILL;

    pipelineDesc.vertexLayout.bindings[0].stride = sizeof(float) * 3;
    pipelineDesc.vertexLayout.bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    pipelineDesc.vertexLayout.attributes[0].binding = 0;
    pipelineDesc.vertexLayout.attributes[0].offset = 0;
    pipelineDesc.vertexLayout.attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;

    pipelineDesc.vertexLayout.bindings[1].stride = sizeof(float) * 8;
    pipelineDesc.vertexLayout.bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    pipelineDesc.vertexLayout.attributes[1].binding = 1;
    pipelineDesc.vertexLayout.attributes[1].offset = 0;
    pipelineDesc.vertexLayout.attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    pipelineDesc.vertexLayout.attributes[2].binding = 1;
    pipelineDesc.vertexLayout.attributes[2].offset = sizeof(float) * 3;
    pipelineDesc.vertexLayout.attributes[2].format = VK_FORMAT_R32G32_SFLOAT;

    pipelineDesc.depthState.depthTest = true;
    pipelineDesc.depthState.depthWrite = true;
    pipelineDesc.depthState.depthFunc = VK_COMPARE_OP_LESS_OR_EQUAL;

    ctx->CreateGraphicsPipeline("modelOpacity", pipelineDesc);
}

void OpacityPass::Execute(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::RenderPass* renderPass = ctx->GetRenderPass("opacity");
    RHI::CmdBeginRenderPass(cmd, renderPass);
    RHI::CmdBindPipeline(cmd, ctx->GetGraphicsPipeline("modelOpacity"));
    RHI::CmdSetViewport(cmd, 0.0f, 0.0f, (float)renderPass->width, (float)renderPass->height, 0.0f, 1.0f);
    RHI::CmdSetScissor(cmd, 0, 0, (int32_t)renderPass->width, (int32_t)renderPass->height);
    RHI::CmdBindBuffer(cmd, 0, ctx->GetBuffer("perFrame"));
    RHI::CmdBindBuffer(cmd, 1, ctx->GetBuffer("gpuScene"));
    OpaqueMeshList::Get()->Execute(cmd);
    RHI::CmdEndRenderPass(cmd);

    RHI::ImageBarrier barrier = {};
    barrier.texture = ctx->GetTexture("opacityColor");
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, barrier);
}

CompositePass::CompositePass()
{
}

CompositePass::~CompositePass()
{
}

void CompositePass::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::RenderPassDesc renderPassDesc = {};
    renderPassDesc.colors[0].texture = ctx->GetTexture("output");
    renderPassDesc.colors[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    renderPassDesc.colors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ctx->CreateRenderPass("composite", renderPassDesc);

    RHI::GraphicsPipelineDesc pipelineDesc = {};
    pipelineDesc.vertexShader = ShaderCache::Get()->GetShader("Assets/Shader/Composite.vert");
    pipelineDesc.fragmentShader = ShaderCache::Get()->GetShader("Assets/Shader/Composite.frag");
    pipelineDesc.renderPass = ctx->GetRenderPass("composite");
    pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineDesc.cullMode = VK_CULL_MODE_NONE;

    ctx->CreateGraphicsPipeline("composite", pipelineDesc);
}

void CompositePass::Execute(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::RenderPass* renderPass = ctx->GetRenderPass("composite");
    RHI::CmdBeginRenderPass(cmd, renderPass);
    RHI::CmdBindPipeline(cmd, ctx->GetGraphicsPipeline("composite"));
    RHI::CmdSetViewport(cmd, 0.0f, 0.0f, (float)renderPass->width, (float)renderPass->height, 0.0f, 1.0f);
    RHI::CmdSetScissor(cmd, 0, 0, (int32_t)renderPass->width, (int32_t)renderPass->height);
    RHI::CmdBindTexture(cmd, 0, ctx->GetTexture("opacityColor"));
    RHI::CmdBindSampler(cmd, 0, ctx->GetSampler("linearClamp"));
    RHI::CmdDraw(cmd, 3, 0);
    RHI::CmdEndRenderPass(cmd);

    RHI::ImageBarrier barrier = {};
    barrier.texture = ctx->GetTexture("opacityColor");
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier.srcAccess = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    RHI::CmdPipelineBarrier(cmd, barrier);
}