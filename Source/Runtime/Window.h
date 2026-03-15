#pragma once

#include <imgui.h>

class GLFWwindow;
class SceneSelectionPanel;

class Window
{
public:
    Window(uint32_t width, uint32_t height, const char* title);
    ~Window();

    bool ShouldClose() const;
    void PollEvents();
    double GetTime() const;
    
    void NewFrame(float dt);
    
    void* GetNativeHandle() const { return m_nativeHandle; }
    ImGuiContext* GetImGuiContext() const { return m_imguiContext; }
    
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    static void InitializeGLFW();
    static void TerminateGLFW();

protected:
    void DrawUI();

private:
    void InitializeImGui();
    
    static void WindowSizeCallback(GLFWwindow* window, int width, int height);
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double x, double y);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

private:
    GLFWwindow* m_glfwWindow = nullptr;
    void* m_nativeHandle = nullptr;
    ImGuiContext* m_imguiContext = nullptr;
    std::unique_ptr<SceneSelectionPanel> m_scenePanel;

    uint32_t m_width;
    uint32_t m_height;
};
