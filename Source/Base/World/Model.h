#pragma once

#include "Foundation/Module.h"
#include "Foundation/ObjectPool.h"
#include "Node.h"

class MeshAsset;
class MaterialAsset;

class Model : public Node
{
public:
    Model();
    ~Model() override;

    void Update(float dt);
    void DirtyTransform() override;

public:
    MeshAsset* meshAsset = nullptr;

private:
    friend class ModelManager;
    uint32_t m_handle;
    uint32_t m_gpuSceneHandle;
    uint32_t m_gpuSceneIndex;
    MeshAsset* m_lastMeshAsset = nullptr;
    std::vector<uint32_t> m_bvhInstanceHandles;
};

class ModelManager : public Module<ModelManager>
{
public:
    Model* CreateModel();
    void DestroyModel(Model* model);

    void Update(float dt);

private:
    ObjectPool<Model> m_pool;
};
