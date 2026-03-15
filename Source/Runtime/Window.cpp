#include "Window.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

Window::Window(uint32_t width, uint32_t height, const char* title)
    : m_width(width), m_height(height)
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
}

void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(io.MouseDown))
        io.MouseDown[button] = true;
    if (action == GLFW_RELEASE && button >= 0 && button < IM_ARRAYSIZE(io.MouseDown))
        io.MouseDown[button] = false;
}

void Window::CursorPosCallback(GLFWwindow* window, double x, double y)
{
}

void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheelH += (float)xoffset;
    io.MouseWheel += (float)yoffset;
}

void Window::DrawUI()
{
    static bool kShowDemoWindow = true;
    if (kShowDemoWindow)
    {
        ImGui::ShowDemoWindow(&kShowDemoWindow);
    }
}