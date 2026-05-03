#include "MeshAsset.h"
#include "AssetManager.h"
#include "Foundation/FileSystem.h"
#include "Foundation/JsonIO.h"
#include "Foundation/VFS.h"
#include "MaterialAsset.h"

struct BakedFileData
{
    uint32_t bvhNodeOffset = 0;
    uint32_t bvhNodeCount = 0;
    uint32_t triOffset = 0;
    uint32_t triCount = 0;
    uint32_t bvhNodeSize = 0;
    uint32_t triSize = 0;
    std::vector<GpuBVHNode> bvhNodes;
    std::vector<GPUTriangle> triangles;
};

static bool ReadBakedFile(const std::string& path, BakedFileData& outData)
{
    std::shared_ptr<FS::File> file = std::shared_ptr<FS::File>(VFS::Open(path, FS::FileMode::Read));
    if (!file || !file->IsOpen())
    {
        return false;
    }

    char magic[4];
    file->Read((uint8_t*)magic, 4);
    if (magic[0] != 'B' || magic[1] != 'A' || magic[2] != 'K' || magic[3] != 'D')
    {
        return false;
    }

    uint32_t version;
    file->Read((uint8_t*)&version, sizeof(uint32_t));
    if (version != 3)
    {
        return false;
    }

    file->Read((uint8_t*)&outData.bvhNodeOffset, sizeof(uint32_t));
    file->Read((uint8_t*)&outData.bvhNodeCount, sizeof(uint32_t));
    file->Read((uint8_t*)&outData.triOffset, sizeof(uint32_t));
    file->Read((uint8_t*)&outData.triCount, sizeof(uint32_t));
    file->Read((uint8_t*)&outData.bvhNodeSize, sizeof(uint32_t));
    file->Read((uint8_t*)&outData.triSize, sizeof(uint32_t));

    if (outData.bvhNodeCount > 0)
    {
        outData.bvhNodes.resize(outData.bvhNodeCount);
        file->Seek(outData.bvhNodeOffset);
        file->Read((uint8_t*)outData.bvhNodes.data(), outData.bvhNodeCount * sizeof(GpuBVHNode));
    }

    if (outData.triCount > 0)
    {
        outData.triangles.resize(outData.triCount);
        file->Seek(outData.triOffset);
        file->Read((uint8_t*)outData.triangles.data(), outData.triCount * sizeof(GPUTriangle));
    }

    return true;
}

static void LoadGpuBVH(RHI::CommandBuffer* cmd, BakedFileData& bakedData, GpuBVH& gpuBVH)
{
    if (!bakedData.bvhNodes.empty())
    {
        RHI::BufferDesc desc = {};
        desc.size = static_cast<uint32_t>(bakedData.bvhNodes.size() * sizeof(GpuBVHNode));
        desc.bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        desc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        RHI::CreateBuffer(desc, gpuBVH.nodeBuffer);
        RHI::CmdUploadBuffer(cmd, gpuBVH.nodeBuffer, desc.size, 0, (void*)bakedData.bvhNodes.data());

        RHI::BufferBarrier barrier = {};
        barrier.buffer = gpuBVH.nodeBuffer;
        barrier.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        barrier.srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        barrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);

        gpuBVH.nodeCount = static_cast<uint32_t>(bakedData.bvhNodes.size());
        gpuBVH.aabbMin = glm::vec3(bakedData.bvhNodes[0].aabbMin);
        gpuBVH.aabbMax = glm::vec3(bakedData.bvhNodes[0].aabbMax);
    }

    if (!bakedData.triangles.empty())
    {
        RHI::BufferDesc desc = {};
        desc.size = static_cast<uint32_t>(bakedData.triangles.size() * sizeof(GPUTriangle));
        desc.bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        desc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        RHI::CreateBuffer(desc, gpuBVH.primitiveBuffer);
        RHI::CmdUploadBuffer(cmd, gpuBVH.primitiveBuffer, desc.size, 0, (void*)bakedData.triangles.data());

        RHI::BufferBarrier barrier = {};
        barrier.buffer = gpuBVH.primitiveBuffer;
        barrier.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        barrier.srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        barrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);

        gpuBVH.primitiveCount = static_cast<uint32_t>(bakedData.triangles.size());
    }
}

MeshAsset::MeshAsset(const std::string& assetPath) : Asset(assetPath)
{
    JsonReader reader(assetPath);

    std::string binPath;
    reader.Field("binary", binPath);
    std::shared_ptr<FS::File> binFile = std::shared_ptr<FS::File>(VFS::Open(binPath, FS::FileMode::Read));
    std::vector<uint8_t> binData(binFile->GetSize());
    binFile->Read(binData.data(), binData.size());

    RHI::CommandBuffer* cmd = RHI::RequestCommandBuffer();
    reader.Array("primitives", [&]() {
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t positionOffset = 0;
        uint32_t positionSize = 0;
        uint32_t attributeOffset = 0;
        uint32_t attributeSize = 0;
        uint32_t indexOffset = 0;
        uint32_t indexSize = 0;
        bool using16uIndex = false;

        reader.Field("vertexCount", vertexCount);
        reader.Field("indexCount", indexCount);
        reader.Field("using16uIndex", using16uIndex);
        reader.Field("positionOffset", positionOffset);
        reader.Field("positionSize", positionSize);
        reader.Field("attributeOffset", attributeOffset);
        reader.Field("attributeSize", attributeSize);
        reader.Field("indexOffset", indexOffset);
        reader.Field("indexSize", indexSize);

        std::string materialPath;
        reader.Field("material", materialPath);

        Primitive primitive;
        primitive.vertexCount = vertexCount;
        primitive.indexCount = indexCount;
        primitive.indexType = using16uIndex ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

        RHI::BufferDesc bufferDesc = {};
        bufferDesc.size = positionSize;
        bufferDesc.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        RHI::CreateBuffer(bufferDesc, primitive.positionBuffer);
        RHI::CmdUploadBuffer(cmd, primitive.positionBuffer, positionSize, 0, binData.data() + positionOffset);

        RHI::BufferBarrier barrier = {};
        barrier.buffer = primitive.positionBuffer;
        barrier.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        barrier.srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        barrier.dstAccess = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);

        bufferDesc.size = attributeSize;
        bufferDesc.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        RHI::CreateBuffer(bufferDesc, primitive.attributeBuffer);
        RHI::CmdUploadBuffer(cmd, primitive.attributeBuffer, attributeSize, 0, binData.data() + attributeOffset);

        barrier.buffer = primitive.attributeBuffer;
        barrier.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        barrier.srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        barrier.dstAccess = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);

        bufferDesc.size = indexSize;
        bufferDesc.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufferDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        RHI::CreateBuffer(bufferDesc, primitive.indexBuffer);
        RHI::CmdUploadBuffer(cmd, primitive.indexBuffer, indexSize, 0, binData.data() + indexOffset);

        barrier.buffer = primitive.indexBuffer;
        barrier.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        barrier.srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        barrier.dstAccess = VK_ACCESS_INDEX_READ_BIT;
        RHI::CmdPipelineBarrier(cmd, barrier);

        if (!materialPath.empty())
        {
            primitive.materialAsset = AssetManager::Get()->Load<MaterialAsset>(materialPath);
        }

        std::string bakedPath;
        if (reader.Field("baked", bakedPath))
        {
            BakedFileData bakedData;
            if (ReadBakedFile(bakedPath, bakedData))
            {
                LoadGpuBVH(cmd, bakedData, primitive.gpuBVH);
                primitive.gpuBVH.nodes = std::move(bakedData.bvhNodes);
                primitive.gpuBVH.triangles = std::move(bakedData.triangles);
            }
        }

        m_primitives.push_back(primitive);
    });
    RHI::Submit(cmd);
}

MeshAsset::~MeshAsset()
{
    for (auto& prim : m_primitives)
    {
        RHI::DestroyBuffer(prim.positionBuffer);
        RHI::DestroyBuffer(prim.attributeBuffer);
        RHI::DestroyBuffer(prim.indexBuffer);

        if (prim.gpuBVH.nodeBuffer)
        {
            RHI::DestroyBuffer(prim.gpuBVH.nodeBuffer);
        }
        if (prim.gpuBVH.primitiveBuffer)
        {
            RHI::DestroyBuffer(prim.gpuBVH.primitiveBuffer);
        }
    }
}
