#include "Passes.h"
#include "Foundation/Var.h"
#include "Math/MathCommon.h"
#include "RenderContext.h"
#include "RenderScene.h"
#include "Renderer.h"
#include "ShaderCache.h"
#include "ShaderRegistry.h"
#include "ShaderSchema.h"

static Var<bool> cv_clusterDebug("cv_clusterDebug", false, "Enable cluster debug visualization");

LightClusterPass::LightClusterPass() {}

LightClusterPass::~LightClusterPass() {}

void LightClusterPass::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    ctx->SetClusterSize(glm::uvec4(CLUSTER_SIZE_X, CLUSTER_SIZE_Y, CLUSTER_SIZE_Z, 0));

    RHI::BufferDesc clustersDesc = {};
    clustersDesc.bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    clustersDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    clustersDesc.size = CLUSTER_COUNT * sizeof(Cluster);
    ctx->CreateBuffer("clusters", clustersDesc);

    RHI::ComputePipelineDesc clusterAABBDesc = {};
    RHI::Shader* clusterAABBShader = ShaderCache::Get()->GetShader(ShaderId::ClusterAABB);
    clusterAABBDesc.computeShader = clusterAABBShader;
    ctx->CreateComputePipeline("clusterAABB", clusterAABBDesc);

    RHI::ComputePipelineDesc clusteringDesc = {};
    RHI::Shader* clusteringShader = ShaderCache::Get()->GetShader(ShaderId::Clustering);
    clusteringDesc.computeShader = clusteringShader;
    ctx->CreateComputePipeline("clustering", clusteringDesc);

    RHI::ComputePipelineDesc debugDesc = {};
    RHI::Shader* clusterDebugShader = ShaderCache::Get()->GetShader(ShaderId::ClusterDebug);
    debugDesc.computeShader = clusterDebugShader;
    ctx->CreateComputePipeline("clusterDebug", debugDesc);
}

void LightClusterPass::Execute(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::Buffer* clusters = ctx->GetBuffer("clusters");

    RHI::BufferBarrier clusterBarrier = {};
    clusterBarrier.buffer = clusters;
    clusterBarrier.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    clusterBarrier.srcAccess = 0;
    clusterBarrier.dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    clusterBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    RHI::CmdPipelineBarrier(cmd, clusterBarrier);

    CreateClusterAABB(ctx, cmd);

    clusterBarrier.srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    clusterBarrier.srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
    clusterBarrier.dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    clusterBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    RHI::CmdPipelineBarrier(cmd, clusterBarrier);

    AssignLightsToClusters(ctx, cmd);

    clusterBarrier.srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    clusterBarrier.srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
    clusterBarrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    clusterBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, clusterBarrier);
}

void LightClusterPass::CreateClusterAABB(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RenderScene* scene = Renderer::Get()->GetScene();
    const RenderView& view = scene->GetPrimaryView();

    RHI::Buffer* clusters = ctx->GetBuffer("clusters");

    ClusterAABBPushConstants pc = {};
    pc.zNearFar = glm::vec2(view.zNear, view.zFar);
    pc.screenSize = ctx->GetScreenSize();
    pc.clusterSize = ctx->GetClusterSize();
    pc.invProj = view.inverseProjection;

    RHI::CmdBindPipeline(cmd, ctx->GetComputePipeline("clusterAABB"));
    RHI::CmdPushConstants(cmd, &pc, sizeof(pc));

    RHI::CmdBindBufferByName(cmd, "s_clusters"_sh, clusters);

    RHI::CmdDispatch(cmd, CLUSTER_SIZE_X, CLUSTER_SIZE_Y, CLUSTER_SIZE_Z);
}

void LightClusterPass::AssignLightsToClusters(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::Pipeline* clusteringPipeline = ctx->GetComputePipeline("clustering");
    RenderScene* scene = Renderer::Get()->GetScene();
    const RenderView& view = scene->GetPrimaryView();

    ClusteringPushConstants pc = {};
    pc.lightCount = glm::uvec2(scene->GetPointLights().Size(), scene->GetSpotLights().Size());
    pc.clusterSize = ctx->GetClusterSize();
    pc.viewMatrix = view.viewMatrix;

    RHI::CmdBindPipeline(cmd, clusteringPipeline);
    RHI::CmdPushConstants(cmd, &pc, sizeof(pc));

    RHI::Buffer* pointLights = ctx->GetBuffer("pointLights");
    RHI::Buffer* spotLights = ctx->GetBuffer("spotLights");
    RHI::Buffer* dirLights = ctx->GetBuffer("dirLights");
    RHI::Buffer* clusters = ctx->GetBuffer("clusters");

    RHI::CmdBindBufferByName(cmd, "s_pointLights"_sh, pointLights);
    RHI::CmdBindBufferByName(cmd, "s_spotLights"_sh, spotLights);
    RHI::CmdBindBufferByName(cmd, "s_dirLights"_sh, dirLights);
    RHI::CmdBindBufferByName(cmd, "s_clusters"_sh, clusters);

    RHI::CmdDispatch(cmd, CLUSTER_SIZE_X, CLUSTER_SIZE_Y, CLUSTER_SIZE_Z);
}

OpacityPass::OpacityPass() {}

OpacityPass::~OpacityPass() {}

static void SetupMeshVertexLayout(RHI::GraphicsPipelineDesc& pipelineDesc)
{
    pipelineDesc.vertexLayout.bindings[0].stride = sizeof(float) * 3;
    pipelineDesc.vertexLayout.bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    pipelineDesc.vertexLayout.attributes[0].binding = 0;
    pipelineDesc.vertexLayout.attributes[0].offset = 0;
    pipelineDesc.vertexLayout.attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;

    pipelineDesc.vertexLayout.bindings[1].stride = sizeof(float) * 8;
    pipelineDesc.vertexLayout.bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    pipelineDesc.vertexLayout.attributes[1].binding = 1;
    pipelineDesc.vertexLayout.attributes[1].offset = 0;
    pipelineDesc.vertexLayout.attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    pipelineDesc.vertexLayout.attributes[2].binding = 1;
    pipelineDesc.vertexLayout.attributes[2].offset = sizeof(float) * 2;
    pipelineDesc.vertexLayout.attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    pipelineDesc.vertexLayout.attributes[3].binding = 1;
    pipelineDesc.vertexLayout.attributes[3].offset = sizeof(float) * 5;
    pipelineDesc.vertexLayout.attributes[3].format = VK_FORMAT_R32G32B32_SFLOAT;
}

void OpacityPass::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    uint32_t width = ctx->GetWidth();
    uint32_t height = ctx->GetHeight();

    RHI::TextureDesc colorDesc = {};
    colorDesc.width = width;
    colorDesc.height = height;
    colorDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ctx->CreateTexture("opacityColor", colorDesc, [&](RHI::Texture* tex) {
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

    RHI::BufferDesc perRenderPassDesc = {};
    perRenderPassDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    perRenderPassDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    perRenderPassDesc.size = sizeof(PerRenderPassData);
    perRenderPassDesc.dynamicBuffer = true;
    RHI::Buffer* perRenderPassBuffer = ctx->CreateBuffer("opacityPerRenderPass", perRenderPassDesc);

    CreateOpaqueMeshPipeline(ctx);
    CreateMaskMeshPipeline(ctx);

    RHI::RenderPass* renderPass = ctx->GetRenderPass("opacity");
    PerRenderPassData perRenderPassData = {};
    perRenderPassData.rtSize = glm::uvec2(renderPass->width, renderPass->height);

    void* perRenderPassMap = RHI::MapMemory(perRenderPassBuffer);
    memcpy(perRenderPassMap, &perRenderPassData, sizeof(PerRenderPassData));
    RHI::UnmapMemory(perRenderPassBuffer);

    RHI::BufferBarrier perRenderPassBarrier = {};
    perRenderPassBarrier.buffer = perRenderPassBuffer;
    perRenderPassBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    perRenderPassBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    perRenderPassBarrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    perRenderPassBarrier.dstAccess = VK_ACCESS_UNIFORM_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, perRenderPassBarrier);
}

void OpacityPass::CreateOpaqueMeshPipeline(RenderContext* ctx)
{
    RHI::GraphicsPipelineDesc pipelineDesc = {};
    pipelineDesc.vertexShader = ShaderCache::Get()->GetShader(ShaderId::ModelVert);
    pipelineDesc.fragmentShader = ShaderCache::Get()->GetShader(ShaderId::ModelFrag);
    pipelineDesc.renderPass = ctx->GetRenderPass("opacity");
    pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineDesc.fillMode = VK_POLYGON_MODE_FILL;

    SetupMeshVertexLayout(pipelineDesc);

    pipelineDesc.depthState.depthTest = true;
    pipelineDesc.depthState.depthWrite = true;
    pipelineDesc.depthState.depthFunc = VK_COMPARE_OP_LESS_OR_EQUAL;

    ctx->CreateGraphicsPipeline("modelOpaque", pipelineDesc);
}

void OpacityPass::CreateMaskMeshPipeline(RenderContext* ctx)
{
    RHI::GraphicsPipelineDesc pipelineDesc = {};
    pipelineDesc.vertexShader = ShaderCache::Get()->GetShader(ShaderId::ModelVert);
    pipelineDesc.fragmentShader = ShaderCache::Get()->GetShader(ShaderId::ModelFrag_AlphaMask);
    pipelineDesc.renderPass = ctx->GetRenderPass("opacity");
    pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineDesc.fillMode = VK_POLYGON_MODE_FILL;

    SetupMeshVertexLayout(pipelineDesc);

    pipelineDesc.depthState.depthTest = true;
    pipelineDesc.depthState.depthWrite = true;
    pipelineDesc.depthState.depthFunc = VK_COMPARE_OP_LESS_OR_EQUAL;

    pipelineDesc.multisampleState.alphaToCoverage = true;

    ctx->CreateGraphicsPipeline("modelMask", pipelineDesc);
}

enum class MeshType
{
    Opaque,
    Mask,
    Blend
};

static void DrawMesh(RenderContext* ctx, RHI::CommandBuffer* cmd, MeshType type)
{
    RHI::Buffer* perFrame = ctx->GetBuffer("perFrame");
    RHI::Buffer* opacityPerRenderPass = ctx->GetBuffer("opacityPerRenderPass");
    RHI::Buffer* alphaPerRenderPass = ctx->GetBuffer("alphaPerRenderPass");
    RHI::Buffer* gpuScene = ctx->GetBuffer("gpuScene");
    RHI::Buffer* pointLights = ctx->GetBuffer("pointLights");
    RHI::Buffer* spotLights = ctx->GetBuffer("spotLights");
    RHI::Buffer* dirLights = ctx->GetBuffer("dirLights");
    RHI::Buffer* clusters = ctx->GetBuffer("clusters");
    RHI::Buffer* gpuMaterials = ctx->GetBuffer("gpuMaterials");

    const char* pipelineName;
    DrawList* drawList;
    switch (type)
    {
        case MeshType::Mask:
            pipelineName = "modelMask";
            drawList = MaskMeshList::Get();
            break;
        case MeshType::Blend:
            pipelineName = "modelBlend";
            drawList = BlendMeshList::Get();
            break;
        default:
            pipelineName = "modelOpaque";
            drawList = OpaqueMeshList::Get();
            break;
    }

    RHI::CmdBindPipeline(cmd, ctx->GetGraphicsPipeline(pipelineName));

    RHI::CmdBindBufferByName(cmd, "u_perFrame"_sh, perFrame);
    RHI::CmdBindBufferByName(cmd, "s_gpuScene"_sh, gpuScene);
    RHI::CmdBindBufferByName(cmd, "s_pointLights"_sh, pointLights);
    RHI::CmdBindBufferByName(cmd, "s_spotLights"_sh, spotLights);
    RHI::CmdBindBufferByName(cmd, "s_dirLights"_sh, dirLights);
    RHI::CmdBindBufferByName(cmd, "s_clusters"_sh, clusters);
    RHI::CmdBindBufferByName(cmd, "s_gpuMaterials"_sh, gpuMaterials);
    RHI::CmdBindBufferByName(cmd, "u_perRenderPass"_sh,
                             type == MeshType::Blend ? alphaPerRenderPass : opacityPerRenderPass);

    drawList->Execute(cmd);
}

void OpacityPass::Execute(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::RenderPass* renderPass = ctx->GetRenderPass("opacity");

    RHI::CmdBeginRenderPass(cmd, renderPass);
    RHI::CmdSetViewport(cmd, 0.0f, 0.0f, (float)renderPass->width, (float)renderPass->height, 0.0f, 1.0f);
    RHI::CmdSetScissor(cmd, 0, 0, (int32_t)renderPass->width, (int32_t)renderPass->height);

    DrawMesh(ctx, cmd, MeshType::Opaque);
    DrawMesh(ctx, cmd, MeshType::Mask);

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

    RHI::ImageBarrier depthBarrier = {};
    depthBarrier.texture = ctx->GetTexture("opacityDepth");
    depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthBarrier.srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthBarrier.srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthBarrier.dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    depthBarrier.dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, depthBarrier);
}

AlphaPass::AlphaPass() {}

AlphaPass::~AlphaPass() {}

void AlphaPass::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    uint32_t width = ctx->GetWidth();
    uint32_t height = ctx->GetHeight();

    RHI::TextureDesc colorDesc = {};
    colorDesc.width = width;
    colorDesc.height = height;
    colorDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ctx->CreateTexture("alphaColor", colorDesc, [&](RHI::Texture* tex) {
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

    RHI::RenderPassDesc renderPassDesc = {};
    renderPassDesc.colors[0].texture = ctx->GetTexture("alphaColor");
    renderPassDesc.colors[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderPassDesc.colors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassDesc.depth.texture = ctx->GetTexture("opacityDepth");
    renderPassDesc.depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderPassDesc.depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ctx->CreateRenderPass("alpha", renderPassDesc);

    CreateBlendMeshPipeline(ctx);

    RHI::BufferDesc perRenderPassDesc = {};
    perRenderPassDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    perRenderPassDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    perRenderPassDesc.size = sizeof(PerRenderPassData);
    perRenderPassDesc.dynamicBuffer = true;
    RHI::Buffer* perRenderPassBuffer = ctx->CreateBuffer("alphaPerRenderPass", perRenderPassDesc);

    RHI::RenderPass* renderPass = ctx->GetRenderPass("alpha");
    PerRenderPassData perRenderPassData = {};
    perRenderPassData.rtSize = glm::uvec2(renderPass->width, renderPass->height);

    void* perRenderPassMap = RHI::MapMemory(perRenderPassBuffer);
    memcpy(perRenderPassMap, &perRenderPassData, sizeof(PerRenderPassData));
    RHI::UnmapMemory(perRenderPassBuffer);

    RHI::BufferBarrier perRenderPassBarrier = {};
    perRenderPassBarrier.buffer = perRenderPassBuffer;
    perRenderPassBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    perRenderPassBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    perRenderPassBarrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    perRenderPassBarrier.dstAccess = VK_ACCESS_UNIFORM_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, perRenderPassBarrier);
}

void AlphaPass::CreateBlendMeshPipeline(RenderContext* ctx)
{
    RHI::GraphicsPipelineDesc pipelineDesc = {};
    pipelineDesc.vertexShader = ShaderCache::Get()->GetShader(ShaderId::ModelVert);
    pipelineDesc.fragmentShader = ShaderCache::Get()->GetShader(ShaderId::ModelFrag_AlphaBlend);
    pipelineDesc.renderPass = ctx->GetRenderPass("alpha");
    pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineDesc.fillMode = VK_POLYGON_MODE_FILL;

    SetupMeshVertexLayout(pipelineDesc);

    pipelineDesc.depthState.depthTest = true;
    pipelineDesc.depthState.depthWrite = false;
    pipelineDesc.depthState.depthFunc = VK_COMPARE_OP_LESS_OR_EQUAL;

    pipelineDesc.blendState.blendEnable = true;
    pipelineDesc.blendState.srcColor = VK_BLEND_FACTOR_SRC_ALPHA;
    pipelineDesc.blendState.dstColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineDesc.blendState.colorOp = VK_BLEND_OP_ADD;
    pipelineDesc.blendState.srcAlpha = VK_BLEND_FACTOR_ONE;
    pipelineDesc.blendState.dstAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineDesc.blendState.alphaOp = VK_BLEND_OP_ADD;

    ctx->CreateGraphicsPipeline("modelBlend", pipelineDesc);
}

void AlphaPass::Execute(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::RenderPass* renderPass = ctx->GetRenderPass("alpha");

    RHI::CmdBeginRenderPass(cmd, renderPass);
    RHI::CmdSetViewport(cmd, 0.0f, 0.0f, (float)renderPass->width, (float)renderPass->height, 0.0f, 1.0f);
    RHI::CmdSetScissor(cmd, 0, 0, (int32_t)renderPass->width, (int32_t)renderPass->height);

    DrawMesh(ctx, cmd, MeshType::Blend);

    RHI::CmdEndRenderPass(cmd);

    RHI::ImageBarrier barrier = {};
    barrier.texture = ctx->GetTexture("alphaColor");
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, barrier);
}

CompositePass::CompositePass() {}

CompositePass::~CompositePass() {}

void CompositePass::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::RenderPassDesc renderPassDesc = {};
    renderPassDesc.colors[0].texture = ctx->GetTexture("output");
    renderPassDesc.colors[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    renderPassDesc.colors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ctx->CreateRenderPass("composite", renderPassDesc);

    RHI::GraphicsPipelineDesc pipelineDesc = {};
    pipelineDesc.vertexShader = ShaderCache::Get()->GetShader(ShaderId::CompositeVert);
    pipelineDesc.fragmentShader = ShaderCache::Get()->GetShader(ShaderId::CompositeFrag);
    pipelineDesc.renderPass = ctx->GetRenderPass("composite");
    pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineDesc.cullMode = VK_CULL_MODE_NONE;

    ctx->CreateGraphicsPipeline("composite", pipelineDesc);
}

void CompositePass::Execute(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::ImageBarrier barrier = {};
    RHI::RenderPass* renderPass = ctx->GetRenderPass("composite");
    RHI::CmdBeginRenderPass(cmd, renderPass);
    RHI::CmdBindPipeline(cmd, ctx->GetGraphicsPipeline("composite"));
    RHI::CmdSetViewport(cmd, 0.0f, 0.0f, (float)renderPass->width, (float)renderPass->height, 0.0f, 1.0f);
    RHI::CmdSetScissor(cmd, 0, 0, (int32_t)renderPass->width, (int32_t)renderPass->height);

    RHI::Buffer* perFrame = ctx->GetBuffer("perFrame");
    RHI::CmdBindBufferByName(cmd, "u_perFrame"_sh, perFrame);

    RHI::CmdBindTextureByName(cmd, "u_opaqueColor"_sh, ctx->GetTexture("opacityColor"));
    RHI::CmdBindSamplerByName(cmd, "u_opaqueColor"_sh, ctx->GetSampler("linearClamp"));
    RHI::CmdBindTextureByName(cmd, "u_alphaColor"_sh, ctx->GetTexture("alphaColor"));
    RHI::CmdBindSamplerByName(cmd, "u_alphaColor"_sh, ctx->GetSampler("linearClamp"));
    RHI::CmdDraw(cmd, 3, 0);
    RHI::CmdEndRenderPass(cmd);

    if (cv_clusterDebug.Get())
    {
        barrier.texture = ctx->GetTexture("output");
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        barrier.dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);

        RHI::CmdBindPipeline(cmd, ctx->GetComputePipeline("clusterDebug"));

        ClusterDebugPushConstants pc = {};
        pc.screenSize = ctx->GetScreenSize();
        pc.pad0 = 0;
        pc.pad1 = 0;
        pc.clusterSize = ctx->GetClusterSize();
        RHI::CmdPushConstants(cmd, &pc, sizeof(pc));

        RHI::Buffer* clusters = ctx->GetBuffer("clusters");
        RHI::CmdBindBufferByName(cmd, "s_clusters"_sh, clusters);
        RHI::CmdBindTextureByName(cmd, "i_screenBuffer"_sh, ctx->GetTexture("output"));

        uint32_t groupsX = (renderPass->width + 7) / 8;
        uint32_t groupsY = (renderPass->height + 7) / 8;
        RHI::CmdDispatch(cmd, groupsX, groupsY, 1);

        barrier.texture = ctx->GetTexture("output");
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        barrier.srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);
    }

    barrier.texture = ctx->GetTexture("opacityColor");
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier.srcAccess = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    RHI::CmdPipelineBarrier(cmd, barrier);

    barrier.texture = ctx->GetTexture("alphaColor");
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier.srcAccess = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    RHI::CmdPipelineBarrier(cmd, barrier);
}

static Var<bool> cv_bvhDebug("cv_bvhDebug", false, "Enable BVH debug ray tracing visualization");

BVHDebugPass::BVHDebugPass() {}

BVHDebugPass::~BVHDebugPass() {}

void BVHDebugPass::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    if (!cv_bvhDebug.Get())
    {
        return;
    }

    RHI::GraphicsPipelineDesc pipelineDesc = {};
    pipelineDesc.vertexShader = ShaderCache::Get()->GetShader(ShaderId::BVHDebugVert);
    pipelineDesc.fragmentShader = ShaderCache::Get()->GetShader(ShaderId::BVHDebugFrag);
    pipelineDesc.renderPass = ctx->GetRenderPass("composite");
    pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineDesc.cullMode = VK_CULL_MODE_NONE;

    ctx->CreateGraphicsPipeline("bvhDebug", pipelineDesc);
}

void BVHDebugPass::Execute(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    if (!cv_bvhDebug.Get())
    {
        return;
    }

    RHI::Buffer* tlasNodes = ctx->GetBuffer("tlasNodes");
    RHI::Buffer* tlasInstances = ctx->GetBuffer("tlasInstances");

    if (!tlasNodes || !tlasInstances)
    {
        return;
    }

    RHI::RenderPass* renderPass = ctx->GetRenderPass("composite");
    RHI::CmdBeginRenderPass(cmd, renderPass);
    RHI::CmdBindPipeline(cmd, ctx->GetGraphicsPipeline("bvhDebug"));
    RHI::CmdSetViewport(cmd, 0.0f, 0.0f, (float)renderPass->width, (float)renderPass->height, 0.0f, 1.0f);
    RHI::CmdSetScissor(cmd, 0, 0, (int32_t)renderPass->width, (int32_t)renderPass->height);

    RHI::Buffer* perFrame = ctx->GetBuffer("perFrame");
    RHI::CmdBindBufferByName(cmd, "u_perFrame"_sh, perFrame);
    RHI::CmdBindBufferByName(cmd, "s_tlasNodes"_sh, tlasNodes);
    RHI::CmdBindBufferByName(cmd, "s_tlasInstances"_sh, tlasInstances);

    RHI::CmdDraw(cmd, 3, 0);
    RHI::CmdEndRenderPass(cmd);
}
