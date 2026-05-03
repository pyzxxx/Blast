#pragma once

#include "BaseRendering.h"
#include "RHI/RHI.h"

struct MeshDrawCall
{
    uint32_t sceneIndex;
    uint32_t materialIndex;

    uint32_t vertexCount;
    uint32_t indexCount;
    bool using16uIndex;

    RHI::Buffer* positionBuffer;
    RHI::Buffer* attributeBuffer;
    RHI::Buffer* indexBuffer;
};

template<>
void ExecuteDrawCall<MeshDrawCall>(RHI::CommandBuffer* cmd, const MeshDrawCall& drawCall);

DRAW_LIST_DECLARE(OpaqueMeshList, MeshDrawCall)
DRAW_LIST_DECLARE(MaskMeshList, MeshDrawCall)
DRAW_LIST_DECLARE(BlendMeshList, MeshDrawCall)