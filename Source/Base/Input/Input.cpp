#include "Input.h"

void Input::Initialize()
{
    m_currentKeyState.fill(false);
    m_previousKeyState.fill(false);
    m_currentButtonState.fill(false);
    m_previousButtonState.fill(false);

    m_mouseX = 0.0f;
    m_mouseY = 0.0f;
    m_previousMouseX = 0.0f;
    m_previousMouseY = 0.0f;
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;

    m_scrollDeltaX = 0.0f;
    m_scrollDeltaY = 0.0f;

    m_cursorVisible = true;
    m_cursorLocked = false;
}

void Input::Terminate()
{
    m_currentKeyState.fill(false);
    m_previousKeyState.fill(false);
    m_currentButtonState.fill(false);
    m_previousButtonState.fill(false);
}

void Input::Update()
{
    m_previousKeyState = m_currentKeyState;
    m_previousButtonState = m_currentButtonState;

    m_previousMouseX = m_mouseX;
    m_previousMouseY = m_mouseY;
    m_mouseDeltaX = m_mouseX - m_previousMouseX;
    m_mouseDeltaY = m_mouseY - m_previousMouseY;

    m_scrollDeltaX = 0.0f;
    m_scrollDeltaY = 0.0f;
}

void Input::SetKeyState(InputKey key, bool pressed)
{
    size_t index = static_cast<size_t>(key);
    if (index < KeyCount)
    {
        m_currentKeyState[index] = pressed;
    }
}

bool Input::IsKeyHeld(InputKey key)
{
    size_t index = static_cast<size_t>(key);
    if (index < KeyCount)
    {
        return m_currentKeyState[index];
    }
    return false;
}

bool Input::IsKeyPressed(InputKey key)
{
    size_t index = static_cast<size_t>(key);
    if (index < KeyCount)
    {
        return m_currentKeyState[index] && !m_previousKeyState[index];
    }
    return false;
}

bool Input::IsKeyReleased(InputKey key)
{
    size_t index = static_cast<size_t>(key);
    if (index < KeyCount)
    {
        return !m_currentKeyState[index] && m_previousKeyState[index];
    }
    return false;
}

void Input::SetMousePosition(float x, float y)
{
    m_mouseX = x;
    m_mouseY = y;
}

void Input::SetMouseButtonState(InputMouseButton button, bool pressed)
{
    size_t index = static_cast<size_t>(button);
    if (index < ButtonCount)
    {
        m_currentButtonState[index] = pressed;
    }
}

void Input::SetScrollDelta(float x, float y)
{
    m_scrollDeltaX += x;
    m_scrollDeltaY += y;
}

float Input::GetMousePositionX()
{
    return m_mouseX;
}

float Input::GetMousePositionY()
{
    return m_mouseY;
}

void Input::GetMousePosition(float* x, float* y)
{
    if (x)
        *x = m_mouseX;
    if (y)
        *y = m_mouseY;
}

void Input::GetMouseDelta(float* x, float* y)
{
    if (x)
        *x = m_mouseDeltaX;
    if (y)
        *y = m_mouseDeltaY;
}

bool Input::IsMouseButtonHeld(InputMouseButton button)
{
    size_t index = static_cast<size_t>(button);
    if (index < ButtonCount)
    {
        return m_currentButtonState[index];
    }
    return false;
}

bool Input::IsMouseButtonPressed(InputMouseButton button)
{
    size_t index = static_cast<size_t>(button);
    if (index < ButtonCount)
    {
        return m_currentButtonState[index] && !m_previousButtonState[index];
    }
    return false;
}

bool Input::IsMouseButtonReleased(InputMouseButton button)
{
    size_t index = static_cast<size_t>(button);
    if (index < ButtonCount)
    {
        return !m_currentButtonState[index] && m_previousButtonState[index];
    }
    return false;
}

void Input::GetScrollDelta(float* x, float* y)
{
    if (x)
        *x = m_scrollDeltaX;
    if (y)
        *y = m_scrollDeltaY;
}

void Input::SetCursorVisible(bool visible)
{
    m_cursorVisible = visible;
}

void Input::SetCursorLocked(bool locked)
{
    m_cursorLocked = locked;
}

bool Input::IsCursorVisible()
{
    return m_cursorVisible;
}

bool Input::IsCursorLocked()
{
    return m_cursorLocked;
}
