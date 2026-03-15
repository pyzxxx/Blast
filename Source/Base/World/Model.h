#pragma once

#include "Node.h"
#include "Foundation/Module.h"
#include "Foundation/ObjectPool.h"

class MeshAsset;

class Model : public Node
{
public:
    Model();
    ~Model() override;

    void Update();

public:
    MeshAsset* meshAsset = nullptr;

private:
    friend class ModelManager;
    uint32_t m_handle;
    uint32_t m_gpuSceneHandle;
    uint32_t m_gpuSceneIndex;
};

class ModelManager : public Module<ModelManager>
{
public:
    Model* CreateModel();
    void DestroyModel(Model* model);

    void Update();

private:
    ObjectPool<Model> m_pool;
};