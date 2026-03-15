#include "Window.h"
#include "SceneSelectionPanel.h"
#include "Input/Input.h"
#include "Input/InputKey.h"
#include "Input/InputMouseButton.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

static InputKey ConvertGLFWToInputKey(int glfwKey)
{
    switch (glfwKey)
    {
        case GLFW_KEY_A: return InputKey::A;
        case GLFW_KEY_B: return InputKey::B;
        case GLFW_KEY_C: return InputKey::C;
        case GLFW_KEY_D: return InputKey::D;
        case GLFW_KEY_E: return InputKey::E;
        case GLFW_KEY_F: return InputKey::F;
        case GLFW_KEY_G: return InputKey::G;
        case GLFW_KEY_H: return InputKey::H;
        case GLFW_KEY_I: return InputKey::I;
        case GLFW_KEY_J: return InputKey::J;
        case GLFW_KEY_K: return InputKey::K;
        case GLFW_KEY_L: return InputKey::L;
        case GLFW_KEY_M: return InputKey::M;
        case GLFW_KEY_N: return InputKey::N;
        case GLFW_KEY_O: return InputKey::O;
        case GLFW_KEY_P: return InputKey::P;
        case GLFW_KEY_Q: return InputKey::Q;
        case GLFW_KEY_R: return InputKey::R;
        case GLFW_KEY_S: return InputKey::S;
        case GLFW_KEY_T: return InputKey::T;
        case GLFW_KEY_U: return InputKey::U;
        case GLFW_KEY_V: return InputKey::V;
        case GLFW_KEY_W: return InputKey::W;
        case GLFW_KEY_X: return InputKey::X;
        case GLFW_KEY_Y: return InputKey::Y;
        case GLFW_KEY_Z: return InputKey::Z;
        case GLFW_KEY_0: return InputKey::Num0;
        case GLFW_KEY_1: return InputKey::Num1;
        case GLFW_KEY_2: return InputKey::Num2;
        case GLFW_KEY_3: return InputKey::Num3;
        case GLFW_KEY_4: return InputKey::Num4;
        case GLFW_KEY_5: return InputKey::Num5;
        case GLFW_KEY_6: return InputKey::Num6;
        case GLFW_KEY_7: return InputKey::Num7;
        case GLFW_KEY_8: return InputKey::Num8;
        case GLFW_KEY_9: return InputKey::Num9;
        case GLFW_KEY_F1: return InputKey::F1;
        case GLFW_KEY_F2: return InputKey::F2;
        case GLFW_KEY_F3: return InputKey::F3;
        case GLFW_KEY_F4: return InputKey::F4;
        case GLFW_KEY_F5: return InputKey::F5;
        case GLFW_KEY_F6: return InputKey::F6;
        case GLFW_KEY_F7: return InputKey::F7;
        case GLFW_KEY_F8: return InputKey::F8;
        case GLFW_KEY_F9: return InputKey::F9;
        case GLFW_KEY_F10: return InputKey::F10;
        case GLFW_KEY_F11: return InputKey::F11;
        case GLFW_KEY_F12: return InputKey::F12;
        case GLFW_KEY_ESCAPE: return InputKey::Escape;
        case GLFW_KEY_ENTER: return InputKey::Enter;
        case GLFW_KEY_TAB: return InputKey::Tab;
        case GLFW_KEY_BACKSPACE: return InputKey::Backspace;
        case GLFW_KEY_INSERT: return InputKey::Insert;
        case GLFW_KEY_DELETE: return InputKey::Delete;
        case GLFW_KEY_HOME: return InputKey::Home;
        case GLFW_KEY_END: return InputKey::End;
        case GLFW_KEY_PAGE_UP: return InputKey::PageUp;
        case GLFW_KEY_PAGE_DOWN: return InputKey::PageDown;
        case GLFW_KEY_RIGHT: return InputKey::Right;
        case GLFW_KEY_LEFT: return InputKey::Left;
        case GLFW_KEY_DOWN: return InputKey::Down;
        case GLFW_KEY_UP: return InputKey::Up;
        case GLFW_KEY_LEFT_SHIFT: return InputKey::LeftShift;
        case GLFW_KEY_RIGHT_SHIFT: return InputKey::RightShift;
        case GLFW_KEY_LEFT_CONTROL: return InputKey::LeftControl;
        case GLFW_KEY_RIGHT_CONTROL: return InputKey::RightControl;
        case GLFW_KEY_LEFT_ALT: return InputKey::LeftAlt;
        case GLFW_KEY_RIGHT_ALT: return InputKey::RightAlt;
        case GLFW_KEY_LEFT_SUPER: return InputKey::LeftSuper;
        case GLFW_KEY_RIGHT_SUPER: return InputKey::RightSuper;
        case GLFW_KEY_SPACE: return InputKey::Space;
        case GLFW_KEY_APOSTROPHE: return InputKey::Apostrophe;
        case GLFW_KEY_COMMA: return InputKey::Comma;
        case GLFW_KEY_MINUS: return InputKey::Minus;
        case GLFW_KEY_PERIOD: return InputKey::Period;
        case GLFW_KEY_SLASH: return InputKey::Slash;
        case GLFW_KEY_SEMICOLON: return InputKey::Semicolon;
        case GLFW_KEY_EQUAL: return InputKey::Equal;
        case GLFW_KEY_LEFT_BRACKET: return InputKey::LeftBracket;
        case GLFW_KEY_BACKSLASH: return InputKey::Backslash;
        case GLFW_KEY_RIGHT_BRACKET: return InputKey::RightBracket;
        case GLFW_KEY_GRAVE_ACCENT: return InputKey::GraveAccent;
        case GLFW_KEY_CAPS_LOCK: return InputKey::CapsLock;
        case GLFW_KEY_SCROLL_LOCK: return InputKey::ScrollLock;
        case GLFW_KEY_NUM_LOCK: return InputKey::NumLock;
        case GLFW_KEY_PRINT_SCREEN: return InputKey::PrintScreen;
        case GLFW_KEY_PAUSE: return InputKey::Pause;
        case GLFW_KEY_KP_0: return InputKey::Keypad0;
        case GLFW_KEY_KP_1: return InputKey::Keypad1;
        case GLFW_KEY_KP_2: return InputKey::Keypad2;
        case GLFW_KEY_KP_3: return InputKey::Keypad3;
        case GLFW_KEY_KP_4: return InputKey::Keypad4;
        case GLFW_KEY_KP_5: return InputKey::Keypad5;
        case GLFW_KEY_KP_6: return InputKey::Keypad6;
        case GLFW_KEY_KP_7: return InputKey::Keypad7;
        case GLFW_KEY_KP_8: return InputKey::Keypad8;
        case GLFW_KEY_KP_9: return InputKey::Keypad9;
        case GLFW_KEY_KP_DECIMAL: return InputKey::KeypadDecimal;
        case GLFW_KEY_KP_DIVIDE: return InputKey::KeypadDivide;
        case GLFW_KEY_KP_MULTIPLY: return InputKey::KeypadMultiply;
        case GLFW_KEY_KP_SUBTRACT: return InputKey::KeypadSubtract;
        case GLFW_KEY_KP_ADD: return InputKey::KeypadAdd;
        case GLFW_KEY_KP_ENTER: return InputKey::KeypadEnter;
        case GLFW_KEY_KP_EQUAL: return InputKey::KeypadEqual;
        default: return InputKey::Invalid;
    }
}

static InputMouseButton ConvertGLFWToInputMouseButton(int glfwButton)
{
    switch (glfwButton)
    {
        case GLFW_MOUSE_BUTTON_LEFT: return InputMouseButton::Left;
        case GLFW_MOUSE_BUTTON_RIGHT: return InputMouseButton::Right;
        case GLFW_MOUSE_BUTTON_MIDDLE: return InputMouseButton::Middle;
        case GLFW_MOUSE_BUTTON_4: return InputMouseButton::Button4;
        case GLFW_MOUSE_BUTTON_5: return InputMouseButton::Button5;
        case GLFW_MOUSE_BUTTON_6: return InputMouseButton::Button6;
        case GLFW_MOUSE_BUTTON_7: return InputMouseButton::Button7;
        case GLFW_MOUSE_BUTTON_8: return InputMouseButton::Button8;
        default: return InputMouseButton::Invalid;
    }
}

Window::Window(uint32_t width, uint32_t height, const char* title)
    : m_width(width)
    , m_height(height)
    , m_scenePanel(std::make_unique<SceneSelectionPanel>())
{
    m_glfwWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);
    m_nativeHandle = glfwGetWin32Window(m_glfwWindow);
    glfwSetWindowUserPointer(m_glfwWindow, this);
    glfwSetFramebufferSizeCallback(m_glfwWindow, WindowSizeCallback);
    glfwSetKeyCallback(m_glfwWindow, KeyCallback);
    glfwSetMouseButtonCallback(m_glfwWindow, MouseButtonCallback);
    glfwSetCursorPosCallback(m_glfwWindow, CursorPosCallback);
    glfwSetScrollCallback(m_glfwWindow, ScrollCallback);
    InitializeImGui();
}

Window::~Window()
{
    ImGui::DestroyContext(m_imguiContext);
    glfwDestroyWindow(m_glfwWindow);
}

void Window::InitializeImGui()
{
    m_imguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_imguiContext);
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendPlatformName = "glfw";
    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;
    io.Fonts->AddFontDefault();
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_glfwWindow);
}

void Window::PollEvents()
{
    glfwPollEvents();
}

double Window::GetTime() const
{
    return glfwGetTime();
}

void Window::NewFrame(float dt)
{
    int w, h;
    glfwGetWindowSize(m_glfwWindow, &w, &h);
    if (w <= 0 || h <= 0)
        return;
    m_width = w;
    m_height = h;
    ImGui::SetCurrentContext(m_imguiContext);
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = dt;
    io.DisplaySize = ImVec2((float)w, (float)h);
    for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
    {
        io.MouseDown[i] = glfwGetMouseButton(m_glfwWindow, i) != 0;
    }
    double mouse_x, mouse_y;
    glfwGetCursorPos(m_glfwWindow, &mouse_x, &mouse_y);
    io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
    ImGui::NewFrame();
    DrawUI();
    ImGui::Render();
}

void Window::InitializeGLFW()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void Window::TerminateGLFW()
{
    glfwTerminate();
}

void Window::WindowSizeCallback(GLFWwindow* window, int width, int height)
{
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->m_width = width;
    self->m_height = height;
}

void Window::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (action == GLFW_PRESS)
        io.KeysDown[key] = true;
    if (action == GLFW_RELEASE)
        io.KeysDown[key] = false;
    io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
    InputKey inputKey = ConvertGLFWToInputKey(key);
    if (inputKey != InputKey::Invalid)
    {
        bool pressed = (action == GLFW_PRESS) || (action == GLFW_REPEAT);
        Input::Get()->SetKeyState(inputKey, pressed);
    }
}

void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(io.MouseDown))
        io.MouseDown[button] = true;
    if (action == GLFW_RELEASE && button >= 0 && button < IM_ARRAYSIZE(io.MouseDown))
        io.MouseDown[button] = false;
    InputMouseButton inputButton = ConvertGLFWToInputMouseButton(button);
    if (inputButton != InputMouseButton::Invalid)
    {
        bool pressed = (action == GLFW_PRESS);
        Input::Get()->SetMouseButtonState(inputButton, pressed);
    }
}

void Window::CursorPosCallback(GLFWwindow* window, double x, double y)
{
    Input::Get()->SetMousePosition(static_cast<float>(x), static_cast<float>(y));
}

void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheelH += (float)xoffset;
    io.MouseWheel += (float)yoffset;
    Input::Get()->SetScrollDelta(static_cast<float>(xoffset), static_cast<float>(yoffset));
}

void Window::DrawUI()
{
    if (m_scenePanel)
    {
        m_scenePanel->Draw();
    }
}
