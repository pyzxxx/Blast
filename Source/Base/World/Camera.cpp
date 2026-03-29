#include "Camera.h"
#include "Rendering/Renderer.h"
#include "Rendering/RenderScene.h"

Camera::Camera()
    : Node()
    , m_projectionType(Perspective)
    , m_fov(60.0f)
    , m_nearPlane(0.1f)
    , m_farPlane(1000.0f)
    , m_orthoWidth(10.0f)
    , m_orthoHeight(10.0f)
{
    RenderScene* scene = Renderer::Get()->GetScene();
    m_renderViewHandle = scene->renderViews.Add();
}

Camera::~Camera()
{
    RenderScene* scene = Renderer::Get()->GetScene();
    scene->renderViews.Remove(m_renderViewHandle);
}

void Camera::Update(float dt)
{
    (void)dt;
    RenderScene* scene = Renderer::Get()->GetScene();
    RenderView* view = scene->renderViews.Get(m_renderViewHandle);
    
    view->viewMatrix = GetViewMatrix();
    view->projectionMatrix = GetProjectionMatrix(16.0f / 9.0f);
    view->viewProjection = view->projectionMatrix * view->viewMatrix;
    view->inverseView = glm::inverse(view->viewMatrix);
    view->inverseProjection = glm::inverse(view->projectionMatrix);
    view->cameraPosition = GetWorldTranslation();
    
    if (m_isPrimary)
    {
        scene->SetPrimaryView(m_renderViewHandle);
    }
}

void Camera::SetPerspective(float fov, float nearPlane, float farPlane)
{
    m_projectionType = Perspective;
    m_fov = fov;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

void Camera::SetOrthographic(float width, float height, float nearPlane, float farPlane)
{
    m_projectionType = Orthographic;
    m_orthoWidth = width;
    m_orthoHeight = height;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

glm::mat4 Camera::GetProjectionMatrix(float aspect)
{
    if (m_projectionType == Perspective)
    {
        glm::mat4 proj = glm::perspective(glm::radians(m_fov), aspect, m_nearPlane, m_farPlane);
        proj[1][1] *= -1;
        return proj;
    }
    else
    {
        float halfWidth = m_orthoWidth * 0.5f;
        float halfHeight = m_orthoHeight * 0.5f;
        glm::mat4 proj = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, m_nearPlane, m_farPlane);
        proj[1][1] *= -1;
        return proj;
    }
}

glm::mat4 Camera::GetViewMatrix()
{
    return glm::inverse(GetWorldTransform());
}

Camera* CameraManager::CreateCamera()
{
    uint32_t handle = m_pool.Add();
    Camera* camera = m_pool.Get(handle);
    camera->m_handle = handle;
    return camera;
}

void CameraManager::DestroyCamera(Camera* camera)
{
    uint32_t handle = camera->m_handle;
    m_pool.Remove(handle);
}

void CameraManager::Update(float dt)
{
    for (uint32_t i = 0; i < m_pool.Size(); i++)
    {
        m_pool[i].Update(dt);
    }
}

Camera* CameraManager::GetPrimaryCamera()
{
    for (uint32_t i = 0; i < m_pool.Size(); ++i)
    {
        Camera* camera = &m_pool[i];
        if (camera->IsPrimary())
            return camera;
    }
    return m_pool.Size() > 0 ? &m_pool[0] : nullptr;
}

void CameraManager::SetPrimaryCamera(Camera* camera)
{
    if (!camera)
        return;
    
    for (uint32_t i = 0; i < m_pool.Size(); ++i)
    {
        Camera* c = &m_pool[i];
        if (c != camera)
        {
            c->MarkPrimary(false);
        }
    }
    
    camera->MarkPrimary(true);
}

std::vector<Camera*> CameraManager::GetAllCameras()
{
    std::vector<Camera*> cameras;
    cameras.reserve(m_pool.Size());
    for (uint32_t i = 0; i < m_pool.Size(); ++i)
    {
        cameras.push_back(&m_pool[i]);
    }
    return cameras;
}
