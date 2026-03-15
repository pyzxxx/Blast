#pragma once

#include "Asset.h"
#include "RHI/RHI.h"

#include <string>

class MeshAsset : public Asset
{
public:
    struct Primitive
    {
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
        RHI::Buffer* positionBuffer = nullptr;
        RHI::Buffer* attributeBuffer = nullptr;
        RHI::Buffer* indexBuffer = nullptr;
    };

    MeshAsset(const std::string& assetPath);
    virtual ~MeshAsset();

    const std::vector<Primitive>& GetPrimitives() { return m_primitives; }

private:
    std::vector<Primitive> m_primitives;
};