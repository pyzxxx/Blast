#include "MeshRendering.h"
#include "ShaderSchema.h"

template<>
void ExecuteDrawCall<MeshDrawCall>(RHI::CommandBuffer* cmd, const MeshDrawCall& drawCall)
{
    RHI::Buffer* vertexBuffers[2] = {drawCall.positionBuffer, drawCall.attributeBuffer};
    RHI::CmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, nullptr);

    VkIndexType indexType = drawCall.using16uIndex ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    RHI::CmdBindIndexBuffer(cmd, drawCall.indexBuffer, 0, indexType);

    MeshPushConstants pushData = {drawCall.sceneIndex, drawCall.materialIndex};
    RHI::CmdPushConstants(cmd, &pushData, sizeof(MeshPushConstants));

    RHI::CmdDrawIndexed(cmd, drawCall.indexCount, 0, 0);
}

DRAW_LIST_IMPLEMENT(OpaqueMeshList, MeshDrawCall)
DRAW_LIST_IMPLEMENT(MaskMeshList, MeshDrawCall)
DRAW_LIST_IMPLEMENT(BlendMeshList, MeshDrawCall)
