#include "BaseModules.h"
#include "Foundation/Module.h"
#include "Foundation/FileSystem.h"
#include "Rendering/Renderer.h"
#include "Window.h"
#include "ImGuiRenderer.h"

int main()
{
    FS::Path::RegisterProtocol("asset", std::string(PROJECT_DIR) + "/Assets/");
    
    Window::InitializeGLFW();
    Window window(1280, 720, "Blast Engine");

    RegisterBaseModules();
    ModuleRegistry::Get().InitializeAll();

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
