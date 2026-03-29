#pragma once

#include "Foundation/Module.h"
#include "World/Camera.h"

class CameraController : public Module<CameraController>
{
public:
    void Initialize() override;
    void Terminate() override;
    void Update(float dt) override;

    Camera* GetCamera() { return m_camera; }
    void SetTargetCamera(Camera* camera);

private:
    void ProcessKeyboardInput(float dt);
    void ProcessMouseInput(float dt);
    void ProcessScrollInput(float dt);

private:
    Camera* m_camera = nullptr;

    float m_movementSpeed = 3.0f;
    float m_mouseSensitivity = 0.002f;

    float m_smoothSpeed = 15.0f;
    float m_rotationSmoothSpeed = 20.0f;

    glm::vec3 m_targetPosition;
    float m_targetYaw = 0.0f;
    float m_targetPitch = 0.0f;

    bool m_mouseCaptured = false;
    bool m_firstMouse = true;
    bool m_warnedNoCamera = false;
};
