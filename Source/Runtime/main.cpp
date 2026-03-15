#include "BaseModules.h"
#include "Foundation/Module.h"
#include "Foundation/FileSystem.h"
#include "Rendering/Renderer.h"
#include "Window.h"
#include "ImGuiRenderer.h"
#include "UserSettings.h"
#include "World/World.h"
#include "SceneBrowser.h"

int main()
{
    FS::Path::RegisterProtocol("asset", std::string(PROJECT_DIR) + "/Assets/");

    Window::InitializeGLFW();
    Window window(1280, 720, "Blast");

    RegisterBaseModules();
    ModuleRegistry::Get().InitializeAll();

    std::string selectedScene = UserSettings::Get().GetSelectedScene();
    if (!selectedScene.empty())
    {
        std::string scenePath = "asset://Scene/" + selectedScene + "/" + selectedScene + ".scene";
        World::Get()->LoadScene(scenePath);
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
        Renderer::Get()->Render();
    }
    
    Renderer::Get()->RemoveExtension(imguiRenderer);
    delete imguiRenderer;

    ModuleRegistry::Get().TerminateAll();
    Window::TerminateGLFW();
    
    return 0;
}
