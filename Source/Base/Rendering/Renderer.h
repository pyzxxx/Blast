#pragma once

#include "Passes.h"
#include "RenderContext.h"
#include "RenderScene.h"
#include "RenderExtension.h"
#include "RHI/RHI.h"
#include "Foundation/Module.h"

class Renderer : public Module<Renderer>
{
public:
    void Initialize() override;
    void Terminate() override;
    
    void SetWindow(void* nativeHandle);
    
    void Render();

    void AddExtension(RenderExtension* extension);
    void RemoveExtension(RenderExtension* extension);

    template<class ListType>
    void AddDrawCall(const typename ListType::DrawCallType& drawCall)
    {
        ListType::Get()->Add(drawCall);
    }

    RenderScene* GetScene() { return m_scene; }
    RenderContext* GetContext() { return m_ctx; }

private:
    void Setup(RHI::CommandBuffer* cmd);
    void Execute(RHI::CommandBuffer* cmd);

private:
    RenderContext* m_ctx = nullptr;
    RenderScene* m_scene = nullptr;
    RHI::Swapchain* m_swapchain = nullptr;
    std::vector<RenderExtension*> m_extensions;

    OpacityPass* m_opacityPass = nullptr;
};
