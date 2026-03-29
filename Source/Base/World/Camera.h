#pragma once

#include "Node.h"
#include "Foundation/Module.h"
#include "Foundation/ObjectPool.h"
#include "Math/MathCommon.h"

class Camera : public Node
{
public:
    enum ProjectionType
    {
        Perspective,
        Orthographic
    };

public:
    Camera();
    ~Camera() override;

    void Update(float dt);

    void SetPerspective(float fov, float nearPlane, float farPlane);
    void SetOrthographic(float width, float height, float nearPlane, float farPlane);

    ProjectionType GetProjectionType() const { return m_projectionType; }
    
    float GetFOV() const { return m_fov; }
    float GetNearPlane() const { return m_nearPlane; }
    float GetFarPlane() const { return m_farPlane; }
    float GetOrthoWidth() const { return m_orthoWidth; }
    float GetOrthoHeight() const { return m_orthoHeight; }

    glm::mat4 GetProjectionMatrix(float aspect);
    glm::mat4 GetViewMatrix();
    
    bool IsPrimary() const { return m_isPrimary; }

private:
    friend class CameraManager;
    
    void MarkPrimary(bool primary) { m_isPrimary = primary; }
    
    uint32_t m_handle;
    uint32_t m_renderViewHandle;

    ProjectionType m_projectionType;
    float m_fov;
    float m_nearPlane;
    float m_farPlane;
    float m_orthoWidth;
    float m_orthoHeight;
    bool m_isPrimary = false;
};

class CameraManager : public Module<CameraManager>
{
public:
    Camera* CreateCamera();
    void DestroyCamera(Camera* camera);

    void Update(float dt);
    
    Camera* GetPrimaryCamera();
    void SetPrimaryCamera(Camera* camera);
    std::vector<Camera*> GetAllCameras();

private:
    ObjectPool<Camera> m_pool;
};
