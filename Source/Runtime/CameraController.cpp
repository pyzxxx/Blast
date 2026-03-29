#include "CameraController.h"
#include "Input/Input.h"
#include "World/Camera.h"
#include "Foundation/Log.h"
#include <imgui.h>

void CameraController::Initialize()
{
    m_warnedNoCamera = false;
}

void CameraController::Terminate()
{
    m_camera = nullptr;
}

void CameraController::SetTargetCamera(Camera* camera)
{
    m_camera = camera;
    m_warnedNoCamera = false;

    if (camera)
    {
        m_targetPosition = camera->GetWorldTranslation();
        glm::vec3 euler = camera->GetLocalEuler();
        m_targetYaw = euler.y;
        m_targetPitch = euler.x;
    }
}

void CameraController::Update(float dt)
{
    Camera* primaryCamera = CameraManager::Get()->GetPrimaryCamera();
    if (!primaryCamera)
    {
        if (!m_warnedNoCamera)
        {
            LOGW("CameraController: No camera bound");
            m_warnedNoCamera = true;
        }
        m_camera = nullptr;
        return;
    }

    if (primaryCamera != m_camera)
    {
        SetTargetCamera(primaryCamera);
    }

    Input* input = Input::Get();

    if (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    ProcessKeyboardInput(dt);

    bool isRightButtonHeld = input->IsMouseButtonHeld(InputMouseButton::Right);

    if (isRightButtonHeld)
    {
        if (!m_mouseCaptured)
        {
            m_mouseCaptured = true;
            m_firstMouse = true;
        }
        ProcessMouseInput(dt);
    }
    else
    {
        m_mouseCaptured = false;
    }

    ProcessScrollInput(dt);

    // Movement
    glm::vec3 currentPosition = m_camera->GetWorldTranslation();
    glm::vec3 smoothedPosition = glm::lerp(currentPosition, m_targetPosition, glm::clamp(m_smoothSpeed * dt, 0.0f, 1.0f));
    m_camera->SetLocalTranslation(smoothedPosition);

    glm::vec3 currentEuler = m_camera->GetLocalEuler();
    float currentYaw = currentEuler.y;
    float currentPitch = currentEuler.x;

    // Rotation
    float deltaYaw = m_targetYaw - currentYaw;
    while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
    while (deltaYaw < -180.0f) deltaYaw += 360.0f;

    float smoothedYaw = currentYaw + deltaYaw * glm::clamp(m_rotationSmoothSpeed * dt, 0.0f, 1.0f);
    float smoothedPitch = glm::lerp(currentPitch, m_targetPitch, glm::clamp(m_rotationSmoothSpeed * dt, 0.0f, 1.0f));

    m_camera->SetLocalEuler(glm::vec3(smoothedPitch, smoothedYaw, 0.0f));
}

void CameraController::ProcessKeyboardInput(float dt)
{
    Input* input = Input::Get();

    float speed = m_movementSpeed;
    if (input->IsKeyHeld(InputKey::LeftShift))
    {
        speed *= 2.0f;
    }

    glm::vec3 forward = m_camera->GetWorldFront();
    glm::vec3 right = m_camera->GetWorldRight();
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    if (input->IsKeyHeld(InputKey::W))
    {
        m_targetPosition += forward * speed * dt;
    }
    if (input->IsKeyHeld(InputKey::S))
    {
        m_targetPosition -= forward * speed * dt;
    }
    if (input->IsKeyHeld(InputKey::A))
    {
        m_targetPosition -= right * speed * dt;
    }
    if (input->IsKeyHeld(InputKey::D))
    {
        m_targetPosition += right * speed * dt;
    }
    if (input->IsKeyHeld(InputKey::Q))
    {
        m_targetPosition += up * speed * dt;
    }
    if (input->IsKeyHeld(InputKey::E))
    {
        m_targetPosition -= up * speed * dt;
    }
}

void CameraController::ProcessMouseInput(float dt)
{
    (void)dt;
    Input* input = Input::Get();

    float deltaX, deltaY;
    input->GetMouseDelta(&deltaX, &deltaY);

    if (m_firstMouse)
    {
        m_firstMouse = false;
        return;
    }

    m_targetYaw -= deltaX * m_mouseSensitivity;
    m_targetPitch -= deltaY * m_mouseSensitivity;

    m_targetPitch = glm::clamp(m_targetPitch, -89.0f, 89.0f);
}

void CameraController::ProcessScrollInput(float dt)
{
    (void)dt;
    Input* input = Input::Get();

    float scrollX, scrollY;
    input->GetScrollDelta(&scrollX, &scrollY);

    float speed = m_movementSpeed * 0.5f;
    if (input->IsKeyHeld(InputKey::LeftShift))
    {
        speed *= 2.0f;
    }

    m_targetPosition += m_camera->GetWorldFront() * scrollY * speed;
}
