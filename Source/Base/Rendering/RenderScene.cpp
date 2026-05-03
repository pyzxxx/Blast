#include "RenderScene.h"
#include "Renderer.h"

#include <algorithm>

RenderScene::RenderScene()
{
    m_nullView.viewMatrix = glm::mat4(1.0f);
    m_nullView.projectionMatrix = glm::mat4(1.0f);
    m_nullView.viewProjection = glm::mat4(1.0f);
    m_nullView.inverseView = glm::mat4(1.0f);
    m_nullView.inverseProjection = glm::mat4(1.0f);
    m_nullView.cameraPosition = glm::vec3(0.0f);
}

RenderScene::~RenderScene() {}

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

uint32_t RenderScene::AddPointLight() { return m_pointLights.Add(); }

void RenderScene::RemovePointLight(uint32_t handle) { m_pointLights.Remove(handle); }

PunctualLight* RenderScene::GetPointLight(uint32_t handle) { return m_pointLights.Get(handle); }

uint32_t RenderScene::AddSpotLight() { return m_spotLights.Add(); }

void RenderScene::RemoveSpotLight(uint32_t handle) { m_spotLights.Remove(handle); }

PunctualLight* RenderScene::GetSpotLight(uint32_t handle) { return m_spotLights.Get(handle); }

uint32_t RenderScene::AddDirectionLight() { return m_dirLights.Add(); }

void RenderScene::RemoveDirectionLight(uint32_t handle) { m_dirLights.Remove(handle); }

DirectionLight* RenderScene::GetDirectionLight(uint32_t handle) { return m_dirLights.Get(handle); }

void RenderScene::ClearBVHInstances()
{
    m_bvhInstances.Clear();
}

uint32_t RenderScene::AddBVHInstance(const GpuBVH* bvh, const glm::mat4& transform)
{
    BVHInstanceEntry entry = {};
    entry.bvh = bvh;
    entry.localToWorld = transform;
    entry.worldToLocal = glm::inverse(transform);
    return m_bvhInstances.Add(entry);
}

void RenderScene::RemoveBVHInstance(uint32_t handle)
{
    m_bvhInstances.Remove(handle);
}

void RenderScene::UpdateBVHInstance(uint32_t handle, const glm::mat4& transform)
{
    BVHInstanceEntry* entry = m_bvhInstances.Get(handle);
    if (entry)
    {
        entry->localToWorld = transform;
        entry->worldToLocal = glm::inverse(transform);
    }
}

void RenderScene::BuildTLAS(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    m_tlasInstances.clear();
    m_tlasNodes.clear();

    if (m_bvhInstances.Size() == 0)
    {
        return;
    }

    for (size_t i = 0; i < m_bvhInstances.Size(); ++i)
    {
        BVHInstanceEntry& inst = m_bvhInstances[i];
        TLASInstance tlasInst = {};
        tlasInst.worldToLocal = inst.worldToLocal;
        tlasInst.localToWorld = inst.localToWorld;
        tlasInst.bvhNodeCount = inst.bvh ? inst.bvh->nodeCount : 0;
        tlasInst.triCount = inst.bvh ? inst.bvh->primitiveCount : 0;
        tlasInst.materialIndex = 0xFFFFFFFF;

        if (inst.bvh && inst.bvh->nodeBuffer)
        {
            uint64_t addr = inst.bvh->nodeBuffer->deviceAddress;
            tlasInst.bvhNodeAddress = glm::uvec2(static_cast<uint32_t>(addr), static_cast<uint32_t>(addr >> 32));
        }
        if (inst.bvh && inst.bvh->primitiveBuffer)
        {
            uint64_t addr = inst.bvh->primitiveBuffer->deviceAddress;
            tlasInst.triAddress = glm::uvec2(static_cast<uint32_t>(addr), static_cast<uint32_t>(addr >> 32));
        }

        m_tlasInstances.push_back(tlasInst);
    }

    auto computeWorldAABB = [&](const BVHInstanceEntry& entry, glm::vec3& outMin, glm::vec3& outMax) {
        outMin = glm::vec3(FLT_MAX);
        outMax = glm::vec3(-FLT_MAX);
        if (!entry.bvh || entry.bvh->nodes.empty())
        {
            return;
        }
        glm::vec3 localMin = entry.bvh->aabbMin;
        glm::vec3 localMax = entry.bvh->aabbMax;
        for (int cx = 0; cx < 2; ++cx)
        {
            for (int cy = 0; cy < 2; ++cy)
            {
                for (int cz = 0; cz < 2; ++cz)
                {
                    glm::vec3 corner = glm::vec3(
                        cx == 0 ? localMin.x : localMax.x,
                        cy == 0 ? localMin.y : localMax.y,
                        cz == 0 ? localMin.z : localMax.z);
                    glm::vec3 worldCorner = glm::vec3(entry.localToWorld * glm::vec4(corner, 1.0f));
                    outMin = glm::min(outMin, worldCorner);
                    outMax = glm::max(outMax, worldCorner);
                }
            }
        }
    };

    struct TLASBuildNode
    {
        uint32_t leftChild = 0xFFFFFFFF;
        uint32_t instanceIndex = 0xFFFFFFFF;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    std::vector<TLASBuildNode> buildNodes;
    buildNodes.reserve(m_tlasInstances.size() * 2);

    TLASBuildNode root = {};
    root.aabbMin = glm::vec3(FLT_MAX);
    root.aabbMax = glm::vec3(-FLT_MAX);
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_bvhInstances.Size()); ++i)
    {
        glm::vec3 instMin;
        glm::vec3 instMax;
        computeWorldAABB(m_bvhInstances[i], instMin, instMax);
        root.aabbMin = glm::min(root.aabbMin, instMin);
        root.aabbMax = glm::max(root.aabbMax, instMax);
    }
    buildNodes.push_back(root);

    std::vector<uint32_t> indices(m_tlasInstances.size());
    for (uint32_t i = 0; i < m_tlasInstances.size(); ++i)
    {
        indices[i] = i;
    }

    auto buildRecursive = [&](auto& self, uint32_t nodeIdx, uint32_t start, uint32_t count) -> void {
        TLASBuildNode& node = buildNodes[nodeIdx];

        if (count <= 1)
        {
            node.leftChild = 0xFFFFFFFF;
            node.instanceIndex = indices[start];
            return;
        }

        glm::vec3 extent = node.aabbMax - node.aabbMin;
        int axis = 0;
        if (extent.y > extent.x)
        {
            axis = 1;
        }
        if (extent.z > extent[axis])
        {
            axis = 2;
        }

        uint32_t mid = start + count / 2;
        std::nth_element(indices.begin() + start, indices.begin() + mid, indices.begin() + start + count,
                         [&](uint32_t a, uint32_t b) {
                             glm::vec3 ca = (m_tlasInstances[a].localToWorld[3]);
                             glm::vec3 cb = (m_tlasInstances[b].localToWorld[3]);
                             return ca[axis] < cb[axis];
                         });

        uint32_t leftChildIdx = static_cast<uint32_t>(buildNodes.size());
        uint32_t rightChildIdx = leftChildIdx + 1;
        node.leftChild = leftChildIdx;
        node.instanceIndex = 0xFFFFFFFF;

        TLASBuildNode leftNode = {};
        leftNode.aabbMin = glm::vec3(FLT_MAX);
        leftNode.aabbMax = glm::vec3(-FLT_MAX);
        for (uint32_t i = start; i < mid; ++i)
        {
            glm::vec3 instMin;
            glm::vec3 instMax;
            computeWorldAABB(m_bvhInstances[indices[i]], instMin, instMax);
            leftNode.aabbMin = glm::min(leftNode.aabbMin, instMin);
            leftNode.aabbMax = glm::max(leftNode.aabbMax, instMax);
        }
        buildNodes.push_back(leftNode);

        TLASBuildNode rightNode = {};
        rightNode.aabbMin = glm::vec3(FLT_MAX);
        rightNode.aabbMax = glm::vec3(-FLT_MAX);
        for (uint32_t i = mid; i < start + count; ++i)
        {
            glm::vec3 instMin;
            glm::vec3 instMax;
            computeWorldAABB(m_bvhInstances[indices[i]], instMin, instMax);
            rightNode.aabbMin = glm::min(rightNode.aabbMin, instMin);
            rightNode.aabbMax = glm::max(rightNode.aabbMax, instMax);
        }
        buildNodes.push_back(rightNode);

        self(self, leftChildIdx, start, mid - start);
        self(self, rightChildIdx, mid, (start + count) - mid);
    };

    buildRecursive(buildRecursive, 0, 0, static_cast<uint32_t>(m_tlasInstances.size()));

    m_tlasNodes.resize(buildNodes.size());
    for (size_t i = 0; i < buildNodes.size(); ++i)
    {
        m_tlasNodes[i].aabbMin = glm::vec4(buildNodes[i].aabbMin, 0.0f);
        m_tlasNodes[i].aabbMax = glm::vec4(buildNodes[i].aabbMax, 0.0f);
        m_tlasNodes[i].leftChild = buildNodes[i].leftChild;
        m_tlasNodes[i].instanceIndex = buildNodes[i].instanceIndex;
    }

    RHI::BufferDesc tlasNodeDesc = {};
    tlasNodeDesc.bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    tlasNodeDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    tlasNodeDesc.size = static_cast<uint32_t>(m_tlasNodes.size() * sizeof(TLASNode));
    tlasNodeDesc.dynamicBuffer = true;
    RHI::Buffer* tlasNodeBuffer = ctx->CreateBuffer("tlasNodes", tlasNodeDesc);

    void* tlasNodeMap = RHI::MapMemory(tlasNodeBuffer);
    memcpy(tlasNodeMap, m_tlasNodes.data(), m_tlasNodes.size() * sizeof(TLASNode));
    RHI::UnmapMemory(tlasNodeBuffer);

    RHI::BufferBarrier tlasNodeBarrier = {};
    tlasNodeBarrier.buffer = tlasNodeBuffer;
    tlasNodeBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    tlasNodeBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    tlasNodeBarrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    tlasNodeBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, tlasNodeBarrier);

    RHI::BufferDesc tlasInstDesc = {};
    tlasInstDesc.bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    tlasInstDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    tlasInstDesc.size = static_cast<uint32_t>(m_tlasInstances.size() * sizeof(TLASInstance));
    tlasInstDesc.dynamicBuffer = true;
    RHI::Buffer* tlasInstanceBuffer = ctx->CreateBuffer("tlasInstances", tlasInstDesc);

    void* tlasInstMap = RHI::MapMemory(tlasInstanceBuffer);
    memcpy(tlasInstMap, m_tlasInstances.data(), m_tlasInstances.size() * sizeof(TLASInstance));
    RHI::UnmapMemory(tlasInstanceBuffer);

    RHI::BufferBarrier tlasInstBarrier = {};
    tlasInstBarrier.buffer = tlasInstanceBuffer;
    tlasInstBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    tlasInstBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    tlasInstBarrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    tlasInstBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, tlasInstBarrier);
}

void RenderScene::Setup(RenderContext* ctx, RHI::CommandBuffer* cmd)
{
    RHI::BufferDesc gpuSceneBufferDesc = {};
    gpuSceneBufferDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    gpuSceneBufferDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    gpuSceneBufferDesc.size = sizeof(GpuSceneNode) * gpuScene.Capacity();
    gpuSceneBufferDesc.dynamicBuffer = true;
    RHI::Buffer* gpuSceneBuffer = ctx->CreateBuffer("gpuScene", gpuSceneBufferDesc);

    uint8_t* gpuSceneMapData = (uint8_t*)RHI::MapMemory(gpuSceneBuffer);
    memcpy(gpuSceneMapData, gpuScene.Data(), sizeof(GpuSceneNode) * gpuScene.Size());
    RHI::UnmapMemory(gpuSceneBuffer);

    RHI::BufferBarrier gpuSceneBarrier = {};
    gpuSceneBarrier.buffer = gpuSceneBuffer;
    gpuSceneBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    gpuSceneBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    gpuSceneBarrier.dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    gpuSceneBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, gpuSceneBarrier);

    RHI::BufferDesc gpuMaterialBufferDesc = {};
    gpuMaterialBufferDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    gpuMaterialBufferDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    gpuMaterialBufferDesc.size = sizeof(GpuMaterialData) * gpuMaterials.Capacity();
    gpuMaterialBufferDesc.dynamicBuffer = true;
    RHI::Buffer* gpuMaterialBuffer = ctx->CreateBuffer("gpuMaterials", gpuMaterialBufferDesc);

    uint8_t* materialMapData = (uint8_t*)RHI::MapMemory(gpuMaterialBuffer);
    memcpy(materialMapData, gpuMaterials.Data(), sizeof(GpuMaterialData) * gpuMaterials.Size());
    RHI::UnmapMemory(gpuMaterialBuffer);

    RHI::BufferBarrier materialBarrier = {};
    materialBarrier.buffer = gpuMaterialBuffer;
    materialBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    materialBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    materialBarrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    materialBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, materialBarrier);

    RHI::BufferDesc pointLightsDesc = {};
    pointLightsDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    pointLightsDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    pointLightsDesc.size = MAX_LIGHT_DATA_STRUCTS * sizeof(PunctualLight);
    pointLightsDesc.dynamicBuffer = true;
    RHI::Buffer* pointLightsBuffer = ctx->CreateBuffer("pointLights", pointLightsDesc);

    uint32_t pointLightCount = glm::min((uint32_t)MAX_LIGHT_DATA_STRUCTS, (uint32_t)m_pointLights.Size());
    if (pointLightCount > 0)
    {
        uint8_t* data = (uint8_t*)RHI::MapMemory(pointLightsBuffer);
        memcpy(data, m_pointLights.Data(), pointLightCount * sizeof(PunctualLight));
        RHI::UnmapMemory(pointLightsBuffer);
    }

    RHI::BufferBarrier pointLightsBarrier = {};
    pointLightsBarrier.buffer = pointLightsBuffer;
    pointLightsBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    pointLightsBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    pointLightsBarrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    pointLightsBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, pointLightsBarrier);

    RHI::BufferDesc spotLightsDesc = {};
    spotLightsDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    spotLightsDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    spotLightsDesc.size = MAX_LIGHT_DATA_STRUCTS * sizeof(PunctualLight);
    spotLightsDesc.dynamicBuffer = true;
    RHI::Buffer* spotLightsBuffer = ctx->CreateBuffer("spotLights", spotLightsDesc);

    uint32_t spotLightCount = glm::min((uint32_t)MAX_LIGHT_DATA_STRUCTS, (uint32_t)m_spotLights.Size());
    if (spotLightCount > 0)
    {
        uint8_t* data = (uint8_t*)RHI::MapMemory(spotLightsBuffer);
        memcpy(data, m_spotLights.Data(), spotLightCount * sizeof(PunctualLight));
        RHI::UnmapMemory(spotLightsBuffer);
    }

    RHI::BufferBarrier spotLightsBarrier = {};
    spotLightsBarrier.buffer = spotLightsBuffer;
    spotLightsBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    spotLightsBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    spotLightsBarrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    spotLightsBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, spotLightsBarrier);

    RHI::BufferDesc dirLightsDesc = {};
    dirLightsDesc.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    dirLightsDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    dirLightsDesc.size = sizeof(DirectionLight);
    dirLightsDesc.dynamicBuffer = true;
    RHI::Buffer* dirLightsBuffer = ctx->CreateBuffer("dirLights", dirLightsDesc);

    uint32_t dirLightCount = glm::min((uint32_t)1, (uint32_t)m_dirLights.Size());
    if (dirLightCount > 0)
    {
        uint8_t* data = (uint8_t*)RHI::MapMemory(dirLightsBuffer);
        memcpy(data, m_dirLights.Data(), dirLightCount * sizeof(DirectionLight));
        RHI::UnmapMemory(dirLightsBuffer);
    }

    RHI::BufferBarrier dirLightsBarrier = {};
    dirLightsBarrier.buffer = dirLightsBuffer;
    dirLightsBarrier.srcStage = VK_PIPELINE_STAGE_HOST_BIT;
    dirLightsBarrier.srcAccess = VK_ACCESS_HOST_WRITE_BIT;
    dirLightsBarrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    dirLightsBarrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, dirLightsBarrier);

    BuildTLAS(ctx, cmd);
}
