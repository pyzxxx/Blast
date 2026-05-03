#include "PCH.h"
#include "GpuBVHBuilder.h"

#include "Math/BoundingBox.h"

#include <algorithm>
#include <cstring>

void GpuBVHBuilder::Build(const Input& input, Output& output)
{
    output.nodes.clear();

    if (input.triCount == 0 || !input.positions || !input.indices)
    {
        return;
    }

    std::vector<TriangleAABB> triangleAABBs(input.triCount);

    for (uint32_t i = 0; i < input.triCount; ++i)
    {
        uint32_t i0 = input.indices[i * 3 + 0];
        uint32_t i1 = input.indices[i * 3 + 1];
        uint32_t i2 = input.indices[i * 3 + 2];

        glm::vec3 p0 = input.positions[i0];
        glm::vec3 p1 = input.positions[i1];
        glm::vec3 p2 = input.positions[i2];

        triangleAABBs[i].min = glm::min(glm::min(p0, p1), p2);
        triangleAABBs[i].max = glm::max(glm::max(p0, p1), p2);
        triangleAABBs[i].centroid = (p0 + p1 + p2) * (1.0f / 3.0f);
    }

    std::vector<uint32_t> indices(input.triCount);
    for (uint32_t i = 0; i < input.triCount; ++i)
    {
        indices[i] = i;
    }

    std::vector<BuildNode> buildNodes;
    buildNodes.reserve(input.triCount * 2);

    BuildNode root = {};
    root.primitiveStart = 0;
    root.primitiveCount = input.triCount;
    root.aabbMin = triangleAABBs[0].min;
    root.aabbMax = triangleAABBs[0].max;
    for (uint32_t i = 1; i < input.triCount; ++i)
    {
        root.aabbMin = glm::min(root.aabbMin, triangleAABBs[i].min);
        root.aabbMax = glm::max(root.aabbMax, triangleAABBs[i].max);
    }
    buildNodes.push_back(root);

    BuildRecursive(buildNodes, indices, triangleAABBs, 0, 0, input.triCount);

    output.nodes.resize(buildNodes.size());
    for (size_t i = 0; i < buildNodes.size(); ++i)
    {
        const BuildNode& src = buildNodes[i];
        GpuBVHNode& dst = output.nodes[i];
        dst.aabbMin = glm::vec4(src.aabbMin, 0.0f);
        dst.aabbMax = glm::vec4(src.aabbMax, 0.0f);
        dst.leftChild = src.leftChild;
        dst.primitiveStart = src.primitiveStart;
        dst.primitiveCount = src.primitiveCount;
        dst.pad = 0;
    }

    output.triangleOrder = std::move(indices);
}

void GpuBVHBuilder::BuildRecursive(std::vector<BuildNode>& buildNodes, std::vector<uint32_t>& indices,
                                   const std::vector<TriangleAABB>& triangleAABBs, uint32_t nodeIdx,
                                   uint32_t start, uint32_t count)
{
    BuildNode& node = buildNodes[nodeIdx];

    if (count <= MAX_LEAF_TRIANGLES)
    {
        node.leftChild = 0xFFFFFFFF;
        node.primitiveStart = start;
        node.primitiveCount = count;
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
                         return triangleAABBs[a].centroid[axis] < triangleAABBs[b].centroid[axis];
                     });

    uint32_t leftCount = mid - start;
    uint32_t rightCount = (start + count) - mid;

    uint32_t leftChildIdx = static_cast<uint32_t>(buildNodes.size());
    uint32_t rightChildIdx = leftChildIdx + 1;
    node.leftChild = leftChildIdx;
    node.primitiveStart = 0;
    node.primitiveCount = 0;

    BuildNode leftNode = {};
    leftNode.aabbMin = triangleAABBs[indices[start]].min;
    leftNode.aabbMax = triangleAABBs[indices[start]].max;
    for (uint32_t i = start + 1; i < mid; ++i)
    {
        leftNode.aabbMin = glm::min(leftNode.aabbMin, triangleAABBs[indices[i]].min);
        leftNode.aabbMax = glm::max(leftNode.aabbMax, triangleAABBs[indices[i]].max);
    }
    buildNodes.push_back(leftNode);

    BuildNode rightNode = {};
    rightNode.aabbMin = triangleAABBs[indices[mid]].min;
    rightNode.aabbMax = triangleAABBs[indices[mid]].max;
    for (uint32_t i = mid + 1; i < start + count; ++i)
    {
        rightNode.aabbMin = glm::min(rightNode.aabbMin, triangleAABBs[indices[i]].min);
        rightNode.aabbMax = glm::max(rightNode.aabbMax, triangleAABBs[indices[i]].max);
    }
    buildNodes.push_back(rightNode);

    BuildRecursive(buildNodes, indices, triangleAABBs, leftChildIdx, start, leftCount);
    BuildRecursive(buildNodes, indices, triangleAABBs, rightChildIdx, mid, rightCount);
}
