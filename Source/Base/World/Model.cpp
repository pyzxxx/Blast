#include "Model.h"
#include "Asset/MeshAsset.h"
#include "Rendering/Renderer.h"
#include "Rendering/MeshRendering.h"

Model::Model()
{
    RenderScene* scene = Renderer::Get()->GetScene();
    m_gpuSceneHandle = scene->gpuScene.Add();
    m_gpuSceneIndex = scene->gpuScene.GetIndex(m_gpuSceneHandle);
}

Model::~Model()
{
    RenderScene* scene = Renderer::Get()->GetScene();
    scene->gpuScene.Remove(m_gpuSceneHandle);
}

void Model::Update()
{
    Renderer* renderer = Renderer::Get();
    RenderScene* scene = renderer->GetScene();
    GpuSceneNode* node = scene->gpuScene.Get(m_gpuSceneHandle);
    node->transform = GetWorldTransform();

    if (meshAsset)
    {
        for (auto& primitive : meshAsset->GetPrimitives())
        {
            MeshDrawCall drawCall = {};
            drawCall.sceneIndex = m_gpuSceneIndex;
            drawCall.materialIndex = 0;
            
            drawCall.vertexCount = primitive.vertexCount;
            drawCall.indexCount = primitive.indexCount;
            drawCall.using16uIndex = (primitive.indexType == VK_INDEX_TYPE_UINT16);
            
            drawCall.positionBuffer = primitive.positionBuffer;
            drawCall.attributeBuffer = primitive.attributeBuffer;
            drawCall.indexBuffer = primitive.indexBuffer;
            
            renderer->AddDrawCall<OpaqueMeshList>(drawCall);
        }
    }
}

Model* ModelManager::CreateModel()
{
    uint32_t modelHandle = m_pool.Add();
    Model* model = m_pool.Get(modelHandle);
    model->m_handle = modelHandle;
    return model;
}

void ModelManager::DestroyModel(Model* model)
{
    uint32_t handle = model->m_handle;
    m_pool.Remove(handle);
}

void ModelManager::Update()
{
    for (uint32_t i = 0; i < m_pool.Size(); i++)
    {
        m_pool[i].Update();
    }
}


