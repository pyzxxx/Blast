#include "Light.h"
#include "PCH.h"
#include "Rendering/RenderScene.h"
#include "Rendering/Renderer.h"
#include <glm/gtx/quaternion.hpp>

Light::Light() {}

Light::~Light() { DestroyLight(); }

void Light::Update(float dt)
{
    Renderer* renderer = Renderer::Get();
    if (!renderer)
    {
        return;
    }

    RenderScene* scene = renderer->GetScene();
    if (!scene)
    {
        return;
    }

    glm::vec3 position = GetWorldTranslation();
    glm::mat4 worldTransform = GetWorldTransform();
    glm::vec3 direction = glm::normalize(glm::vec3(worldTransform[2]));

    if (type == LightType::Point || type == LightType::Spot)
    {
        PunctualLight* lightObj = nullptr;

        if (type == LightType::Point)
        {
            if (m_renderHandle == INVALID_HANDLE)
            {
                m_renderHandle = scene->AddPointLight();
            }
            lightObj = scene->GetPointLight(m_renderHandle);
        }
        else
        {
            if (m_renderHandle == INVALID_HANDLE)
            {
                m_renderHandle = scene->AddSpotLight();
            }
            lightObj = scene->GetSpotLight(m_renderHandle);
        }

        if (lightObj)
        {
            float radius = glm::max(0.001f, range);
            lightObj->invRadius = 1.0f / radius;
            lightObj->color = color;
            lightObj->intensity = intensity;
            lightObj->position = position;
            lightObj->direction = direction;
            lightObj->coneAngle = glm::radians(spotAngle);
            lightObj->innerAngle = glm::radians(innerAngle);
        }
    }
    else if (type == LightType::Direction)
    {
        if (m_renderHandle == INVALID_HANDLE)
        {
            m_renderHandle = scene->AddDirectionLight();
        }

        DirectionLight* lightObj = scene->GetDirectionLight(m_renderHandle);
        if (lightObj)
        {
            lightObj->color = color;
            lightObj->intensity = intensity;
            lightObj->direction = direction;
            lightObj->hasShadow = hasShadow;
            lightObj->shadow = 0;
        }
    }
}

void Light::DestroyLight()
{
    if (m_renderHandle == INVALID_HANDLE)
    {
        return;
    }

    Renderer* renderer = Renderer::Get();
    if (!renderer)
    {
        return;
    }

    RenderScene* scene = renderer->GetScene();
    if (!scene)
    {
        return;
    }

    if (type == LightType::Point)
    {
        scene->RemovePointLight(m_renderHandle);
    }
    else if (type == LightType::Spot)
    {
        scene->RemoveSpotLight(m_renderHandle);
    }
    else if (type == LightType::Direction)
    {
        scene->RemoveDirectionLight(m_renderHandle);
    }

    m_renderHandle = INVALID_HANDLE;
}

Light* LightManager::CreateLight()
{
    uint32_t handle = m_pool.Add();
    Light* light = m_pool.Get(handle);
    light->m_handle = handle;
    return light;
}

void LightManager::DestroyLight(Light* light)
{
    uint32_t handle = light->m_handle;
    m_pool.Remove(handle);
}

void LightManager::Update(float dt)
{
    for (uint32_t i = 0; i < m_pool.Size(); i++)
    {
        m_pool[i].Update(dt);
    }
}
