#include "MeshAsset.h"
#include "Foundation/FileSystem.h"
#include "Foundation/JsonIO.h"
#include "Foundation/VFS.h"

MeshAsset::MeshAsset(const std::string& assetPath)
    : Asset(assetPath)
{
    JsonReader reader(assetPath);

    std::string binPath;
    reader.Field("binary", binPath);
    std::shared_ptr<FS::File> binFile = std::shared_ptr<FS::File>(VFS::Open(binPath, FS::FileMode::Read));
    std::vector<uint8_t> binData(binFile->GetSize());
    binFile->Read(binData.data(), binData.size());

    RHI::CommandBuffer* cmd = RHI::RequestCommandBuffer();
    reader.Array("primitives", [&](){
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
    }
}
