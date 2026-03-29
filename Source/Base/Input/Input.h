#pragma once

#include "Foundation/Module.h"
#include "InputKey.h"
#include "InputMouseButton.h"

class Input : public Module<Input>
{
public:
    void Initialize() override;
    void Terminate() override;
    void Update(float dt) override;

    void SetKeyState(InputKey key, bool pressed);
    bool IsKeyHeld(InputKey key);
    bool IsKeyPressed(InputKey key);
    bool IsKeyReleased(InputKey key);

    void SetMousePosition(float x, float y);
    void SetMouseDeltaDirect(float x, float y);
    void SetMouseButtonState(InputMouseButton button, bool pressed);
    void SetScrollDelta(float x, float y);

    float GetMousePositionX();
    float GetMousePositionY();
    void GetMousePosition(float* x, float* y);
    void GetMouseDelta(float* x, float* y);

    bool IsMouseButtonHeld(InputMouseButton button);
    bool IsMouseButtonPressed(InputMouseButton button);
    bool IsMouseButtonReleased(InputMouseButton button);

    void GetScrollDelta(float* x, float* y);

    void SetCursorVisible(bool visible);
    void SetCursorLocked(bool locked);
    bool IsCursorVisible();
    bool IsCursorLocked();

private:
    static constexpr size_t KeyCount = static_cast<size_t>(InputKey::MaxKey);
    static constexpr size_t ButtonCount = static_cast<size_t>(InputMouseButton::MaxButton);

    std::array<bool, KeyCount> m_currentKeyState = {};
    std::array<bool, KeyCount> m_previousKeyState = {};
    std::array<bool, ButtonCount> m_currentButtonState = {};
    std::array<bool, ButtonCount> m_previousButtonState = {};

    float m_mouseX = 0.0f;
    float m_mouseY = 0.0f;
    float m_previousMouseX = 0.0f;
    float m_previousMouseY = 0.0f;
    float m_mouseDeltaX = 0.0f;
    float m_mouseDeltaY = 0.0f;

    float m_scrollDeltaX = 0.0f;
    float m_scrollDeltaY = 0.0f;

    bool m_cursorVisible = true;
    bool m_cursorLocked = false;
};
