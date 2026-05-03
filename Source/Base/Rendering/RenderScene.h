#pragma once

#include "Acceleration/GpuBVHBuilder.h"
#include "Foundation/ObjectPool.h"
#include "Math/MathCommon.h"
#include "RHI/RHI.h"
#include "RenderContext.h"
#include "ShaderSchema.h"

enum class LightType
{
    Point,
    Spot,
    Direction
};

struct RenderView
{
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 viewProjection;
    glm::mat4 inverseView;
    glm::mat4 inverseProjection;
    glm::vec3 cameraPosition;
    float zNear;
    float zFar;
    float exposure;
};

struct BVHInstanceEntry
{
    const GpuBVH* bvh = nullptr;
    glm::mat4 localToWorld;
    glm::mat4 worldToLocal;
};

class RenderScene
{
public:
    RenderScene();
    ~RenderScene();

    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd);

    const RenderView& GetPrimaryView() const;
    void SetPrimaryView(uint32_t renderViewHandle);

    uint32_t AddPointLight();
    void RemovePointLight(uint32_t handle);
    PunctualLight* GetPointLight(uint32_t handle);
    ObjectPool<PunctualLight>& GetPointLights() { return m_pointLights; }

    uint32_t AddSpotLight();
    void RemoveSpotLight(uint32_t handle);
    PunctualLight* GetSpotLight(uint32_t handle);
    ObjectPool<PunctualLight>& GetSpotLights() { return m_spotLights; }

    uint32_t AddDirectionLight();
    void RemoveDirectionLight(uint32_t handle);
    DirectionLight* GetDirectionLight(uint32_t handle);
    ObjectPool<DirectionLight>& GetDirectionLights() { return m_dirLights; }

    void ClearBVHInstances();
    uint32_t AddBVHInstance(const GpuBVH* bvh, const glm::mat4& transform);
    void RemoveBVHInstance(uint32_t handle);
    void UpdateBVHInstance(uint32_t handle, const glm::mat4& transform);


    ObjectPool<GpuMaterialData> gpuMaterials;
    ObjectPool<GpuSceneNode> gpuScene;
    ObjectPool<RenderView> renderViews;

private:
    void BuildTLAS(RenderContext* ctx, RHI::CommandBuffer* cmd);

    RenderView m_nullView;
    uint32_t m_primaryViewHandle = 0;
    ObjectPool<PunctualLight> m_pointLights;
    ObjectPool<PunctualLight> m_spotLights;
    ObjectPool<DirectionLight> m_dirLights;

    ObjectPool<BVHInstanceEntry> m_bvhInstances;
    std::vector<TLASNode> m_tlasNodes;
    std::vector<TLASInstance> m_tlasInstances;
};
