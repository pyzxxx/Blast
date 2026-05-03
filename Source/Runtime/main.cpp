#include "BaseModules.h"
#include "Foundation/Module.h"
#include "Foundation/VFS.h"
#include "Foundation/Var.h"
#include "ImGuiRenderer.h"
#include "Rendering/Renderer.h"
#include "RuntimeModules.h"
#include "SceneBrowser.h"
#include "UserSettings.h"
#include "Window.h"
#include "World/World.h"

int main()
{
    VFS::Mount("Assets", std::string(PROJECT_DIR) + "/Assets/");

    Window::InitializeGLFW();
    Window window(1280, 720, "Blast");

    RegisterBaseModules();
    RegisterRuntimeModules();
    ModuleRegistry::Get().InitializeAll();

    Renderer::Get()->SetWindow(window.GetNativeHandle());

    std::string selectedScene = UserSettings::Get().GetSelectedScene();
    if (!selectedScene.empty())
    {
        std::string sceneDir = VFS::Join("Assets/Scenes", selectedScene);
        World::Get()->Load(sceneDir);
        SceneBrowser::Get().SetCurrentScene(selectedScene);
    }

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
