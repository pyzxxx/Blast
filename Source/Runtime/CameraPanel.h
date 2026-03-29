#pragma once

#include "Panel.h"
#include <vector>

class Camera;

class CameraPanel : public Panel
{
public:
    CameraPanel();

    const char* GetName() const override { return "Camera"; }
    void DrawContent() override;

private:
    void ControlSelectedCamera(const std::vector<Camera*>& cameras);
    void SetSelectedAsPrimary(const std::vector<Camera*>& cameras);

private:
    int m_selectedCameraIndex;
};