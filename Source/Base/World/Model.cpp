#include "Model.h"
#include "Asset/MaterialAsset.h"
#include "Asset/MeshAsset.h"
#include "Rendering/MeshRendering.h"
#include "Rendering/Renderer.h"

static MaterialBlendMode GetMaterialBlendMode(MaterialAsset* material)
{
    if (!material)
    {
        return MaterialBlendMode::Opaque;
    }
    return material->GetBlendMode();
}

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
    for (uint32_t handle : m_bvhInstanceHandles)
    {
        scene->RemoveBVHInstance(handle);
    }
    m_bvhInstanceHandles.clear();
}

void Model::DirtyTransform() {}

void Model::Update(float dt)
{
    Renderer* renderer = Renderer::Get();
    RenderScene* scene = renderer->GetScene();
    GpuSceneNode* node = scene->gpuScene.Get(m_gpuSceneHandle);
    node->transform = GetWorldTransform();

    if (meshAsset)
    {
        if (meshAsset != m_lastMeshAsset)
        {
            for (uint32_t handle : m_bvhInstanceHandles)
            {
                scene->RemoveBVHInstance(handle);
            }
            m_bvhInstanceHandles.clear();

            for (auto& primitive : meshAsset->GetPrimitives())
            {
                if (primitive.gpuBVH.nodeCount > 0)
                {
                    uint32_t handle = scene->AddBVHInstance(&primitive.gpuBVH, GetWorldTransform());
                    m_bvhInstanceHandles.push_back(handle);
                }
            }

            m_lastMeshAsset = meshAsset;
        }
        else
        {
            for (uint32_t handle : m_bvhInstanceHandles)
            {
                scene->UpdateBVHInstance(handle, GetWorldTransform());
            }
        }

        for (auto& primitive : meshAsset->GetPrimitives())
        {
            MeshDrawCall drawCall = {};
            drawCall.sceneIndex = m_gpuSceneIndex;
            drawCall.materialIndex = primitive.materialAsset ? primitive.materialAsset->GetGpuMaterialIndex() : 0;

            drawCall.vertexCount = primitive.vertexCount;
            drawCall.indexCount = primitive.indexCount;
            drawCall.using16uIndex = (primitive.indexType == VK_INDEX_TYPE_UINT16);

            drawCall.positionBuffer = primitive.positionBuffer;
            drawCall.attributeBuffer = primitive.attributeBuffer;
            drawCall.indexBuffer = primitive.indexBuffer;

            MaterialBlendMode blendMode = GetMaterialBlendMode(primitive.materialAsset);
            if (blendMode == MaterialBlendMode::Mask)
            {
                renderer->AddDrawCall<MaskMeshList>(drawCall);
            }
            else if (blendMode == MaterialBlendMode::Blend)
            {
                renderer->AddDrawCall<BlendMeshList>(drawCall);
            }
            else
            {
                renderer->AddDrawCall<OpaqueMeshList>(drawCall);
            }
        }
    }
    else if (m_lastMeshAsset)
    {
        for (uint32_t handle : m_bvhInstanceHandles)
        {
            scene->RemoveBVHInstance(handle);
        }
        m_bvhInstanceHandles.clear();
        m_lastMeshAsset = nullptr;
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

void ModelManager::Update(float dt)
{
    for (uint32_t i = 0; i < m_pool.Size(); i++)
    {
        m_pool[i].Update(dt);
    }
}
