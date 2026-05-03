#pragma once

#include "Foundation/Module.h"
#include "Foundation/ObjectPool.h"
#include "Node.h"
#include "Rendering/RenderScene.h"

class Light : public Node
{
public:
    Light();
    ~Light() override;

    void Update(float dt);

public:
    LightType type = LightType::Point;
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 5.0f;
    float spotAngle = 45.0f;
    float innerAngle = 40.0f;
    bool hasShadow = false;

private:
    void DestroyLight();

    friend class LightManager;
    uint32_t m_handle = INVALID_HANDLE;
    uint32_t m_renderHandle = INVALID_HANDLE;
};

class LightManager : public Module<LightManager>
{
public:
    Light* CreateLight();
    void DestroyLight(Light* light);

    void Update(float dt);

private:
    ObjectPool<Light> m_pool;
};
