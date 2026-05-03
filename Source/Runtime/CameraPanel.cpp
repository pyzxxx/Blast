#include "CameraPanel.h"
#include "CameraController.h"
#include "Foundation/Log.h"
#include "World/Camera.h"
#include <imgui.h>

CameraPanel::CameraPanel() : m_selectedCameraIndex(-1) {}

void CameraPanel::DrawContent()
{
    CameraManager* cameraManager = CameraManager::Get();
    CameraController* controller = CameraController::Get();

    if (!cameraManager || !controller)
    {
        ImGui::Text("Camera system not available");
        return;
    }

    std::vector<Camera*> cameras = cameraManager->GetAllCameras();
    Camera* currentCamera = controller->GetCamera();

    if (currentCamera && m_selectedCameraIndex >= 0)
    {
        bool found = false;
        for (size_t i = 0; i < cameras.size(); ++i)
        {
            if (cameras[i] == currentCamera)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            m_selectedCameraIndex = -1;
        }
    }

    ImGui::Text("Current Camera:");
    if (currentCamera)
    {
        int currentIndex = -1;
        for (size_t i = 0; i < cameras.size(); ++i)
        {
            if (cameras[i] == currentCamera)
            {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        if (currentIndex >= 0)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "Camera %d", currentIndex + 1);
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", buf);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Position:");

        glm::vec3 pos = currentCamera->GetWorldTranslation();
        ImGui::Text("  X: %.2f", pos.x);
        ImGui::Text("  Y: %.2f", pos.y);
        ImGui::Text("  Z: %.2f", pos.z);

        ImGui::Spacing();
        ImGui::Text("Rotation (Euler):");

        glm::vec3 euler = currentCamera->GetLocalEuler();
        ImGui::Text("  Pitch: %.1f", euler.x);
        ImGui::Text("  Yaw:   %.1f", euler.y);
        ImGui::Text("  Roll:  %.1f", euler.z);

        ImGui::Spacing();
        ImGui::Text("Direction:");

        glm::vec3 front = currentCamera->GetWorldFront();
        ImGui::Text("  X: %.3f", front.x);
        ImGui::Text("  Y: %.3f", front.y);
        ImGui::Text("  Z: %.3f", front.z);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Projection:");

        if (currentCamera->GetProjectionType() == Camera::Perspective)
        {
            ImGui::Text("  Type: Perspective");
            ImGui::Text("  FOV:  %.1f", currentCamera->GetFOV());
        }
        else
        {
            ImGui::Text("  Type: Orthographic");
        }
        ImGui::Text("  Near: %.2f", currentCamera->GetNearPlane());
        ImGui::Text("  Far:  %.2f", currentCamera->GetFarPlane());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Exposure:");

        float aperture = currentCamera->GetAperture();
        if (ImGui::SliderFloat("Aperture (f-stop)", &aperture, 1.0f, 22.0f, "f/%.1f"))
        {
            currentCamera->SetAperture(aperture);
        }

        float shutterSpeed = currentCamera->GetShutterSpeed();
        float shutterInv = 1.0f / glm::max(shutterSpeed, 0.0001f);
        if (ImGui::SliderFloat("Shutter Speed (1/s)", &shutterInv, 1.0f, 4000.0f, "1/%.0f"))
        {
            currentCamera->SetShutterSpeed(1.0f / shutterInv);
        }

        float iso = currentCamera->GetISO();
        if (ImGui::SliderFloat("ISO", &iso, 100.0f, 6400.0f, "%.0f"))
        {
            currentCamera->SetISO(iso);
        }

        float evComp = currentCamera->GetEV100Compensation();
        if (ImGui::SliderFloat("EV Compensation", &evComp, -3.0f, 3.0f, "%.1f"))
        {
            currentCamera->SetEV100Compensation(evComp);
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "None");
    }

    ImGui::Separator();
    ImGui::Text("Available Cameras:");

    if (cameras.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No cameras found");
    }
    else
    {
        ImGui::BeginChild("CameraList", ImVec2(0, 200), true);
        for (size_t i = 0; i < cameras.size(); ++i)
        {
            Camera* camera = cameras[i];
            if (!camera)
            {
                continue;
            }

            char label[64];
            bool isPrimary = camera->IsPrimary();

            if (isPrimary)
            {
                snprintf(label, sizeof(label), "Camera %zu [PRIMARY]", i + 1);
            }
            else
            {
                snprintf(label, sizeof(label), "Camera %zu", i + 1);
            }

            bool isSelected = (static_cast<int>(i) == m_selectedCameraIndex);
            if (ImGui::Selectable(label, isSelected))
            {
                m_selectedCameraIndex = static_cast<int>(i);
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        bool canSelect = m_selectedCameraIndex >= 0 && m_selectedCameraIndex < static_cast<int>(cameras.size());

        if (canSelect)
        {
            if (ImGui::Button("Control Selected Camera", ImVec2(200, 30)))
            {
                ControlSelectedCamera(cameras);
            }

            ImGui::Spacing();

            Camera* selectedCamera = cameras[m_selectedCameraIndex];
            if (selectedCamera && !selectedCamera->IsPrimary())
            {
                if (ImGui::Button("Set as Primary Camera", ImVec2(200, 30)))
                {
                    SetSelectedAsPrimary(cameras);
                }
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("Control Selected Camera", ImVec2(200, 30));
            ImGui::Spacing();
            ImGui::Button("Set as Primary Camera", ImVec2(200, 30));
            ImGui::EndDisabled();
        }
    }
}

void CameraPanel::ControlSelectedCamera(const std::vector<Camera*>& cameras)
{
    CameraController* controller = CameraController::Get();

    if (!controller)
    {
        return;
    }

    if (m_selectedCameraIndex < 0 || m_selectedCameraIndex >= static_cast<int>(cameras.size()))
    {
        return;
    }

    Camera* camera = cameras[m_selectedCameraIndex];
    if (camera)
    {
        controller->SetTargetCamera(camera);
        LOGI("CameraPanel: Now controlling Camera %d", m_selectedCameraIndex + 1);
    }
}

void CameraPanel::SetSelectedAsPrimary(const std::vector<Camera*>& cameras)
{
    CameraManager* cameraManager = CameraManager::Get();

    if (!cameraManager)
    {
        return;
    }

    if (m_selectedCameraIndex < 0 || m_selectedCameraIndex >= static_cast<int>(cameras.size()))
    {
        return;
    }

    Camera* selectedCamera = cameras[m_selectedCameraIndex];
    if (selectedCamera)
    {
        cameraManager->SetPrimaryCamera(selectedCamera);
        LOGI("CameraPanel: Set Camera %d as primary", m_selectedCameraIndex + 1);
    }
}
