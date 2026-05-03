#pragma once

#include "RHI/RHI.h"
#include "RayTracingInterop.h"

#include <vector>

struct GpuBVH
{
    RHI::Buffer* nodeBuffer = nullptr;
    RHI::Buffer* primitiveBuffer = nullptr;
    uint32_t nodeCount = 0;
    uint32_t primitiveCount = 0;

    std::vector<GpuBVHNode> nodes;
    std::vector<GPUTriangle> triangles;
    glm::vec3 aabbMin = glm::vec3(0.0f);
    glm::vec3 aabbMax = glm::vec3(0.0f);
};

struct GpuTLAS
{
    RHI::Buffer* nodeBuffer = nullptr;
    RHI::Buffer* instanceBuffer = nullptr;
    uint32_t nodeCount = 0;
    uint32_t instanceCount = 0;
};

class GpuBVHBuilder
{
public:
    struct Input
    {
        const glm::vec3* positions = nullptr;
        const uint32_t* indices = nullptr;
        uint32_t triCount = 0;
    };

    struct Output
    {
        std::vector<GpuBVHNode> nodes;
        std::vector<uint32_t> triangleOrder;
    };

    void Build(const Input& input, Output& output);

private:
    struct TriangleAABB
    {
        glm::vec3 min;
        glm::vec3 max;
        glm::vec3 centroid;
    };

    struct BuildNode
    {
        uint32_t leftChild = 0xFFFFFFFF;
        uint32_t primitiveStart = 0;
        uint32_t primitiveCount = 0;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    void BuildRecursive(std::vector<BuildNode>& buildNodes, std::vector<uint32_t>& indices,
                        const std::vector<TriangleAABB>& triangleAABBs, uint32_t nodeIdx,
                        uint32_t start, uint32_t count);

    static constexpr uint32_t MAX_LEAF_TRIANGLES = 8;
};
