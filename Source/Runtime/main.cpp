#include "BaseModules.h"
#include "RuntimeModules.h"
#include "Foundation/Module.h"
#include "Foundation/VFS.h"
#include "Rendering/Renderer.h"
#include "Window.h"
#include "ImGuiRenderer.h"
#include "UserSettings.h"
#include "World/World.h"
#include "SceneBrowser.h"

int main()
{
    VFS::Mount("Assets", std::string(PROJECT_DIR) + "/Assets/");

    Window::InitializeGLFW();
    Window window(1280, 720, "Blast");

    RegisterBaseModules();
    RegisterRuntimeModules();
    ModuleRegistry::Get().InitializeAll();

    std::string selectedScene = UserSettings::Get().GetSelectedScene();
    if (!selectedScene.empty())
    {
        std::string sceneDir = VFS::Join("Assets/Scene", selectedScene);
        World::Get()->Load(sceneDir);
        SceneBrowser::Get().SetCurrentScene(selectedScene);
    }

    Renderer::Get()->SetWindow(window.GetNativeHandle());

    ImGuiRenderer* imguiRenderer = new ImGuiRenderer();
    Renderer::Get()->AddExtension(imguiRenderer);

    double lastTime = 0.0;
    while (!window.ShouldClose())
    {
        window.PollEvents();

        double currentTime = window.GetTime();
        float dt = (float)(currentTime - lastTime);
        lastTime = currentTime;

        window.NewFrame(dt);
        ModuleRegistry::Get().UpdateAll(dt);
        Renderer::Get()->Render();
    }

    Renderer::Get()->RemoveExtension(imguiRenderer);
    delete imguiRenderer;

    ModuleRegistry::Get().TerminateAll();
    Window::TerminateGLFW();

    return 0;
}
