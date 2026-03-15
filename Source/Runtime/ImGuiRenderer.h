#pragma once

#include "RHI/RHI.h"
#include "Rendering/RenderExtension.h"
#include "Rendering/BaseRendering.h"

#include <memory>

class RenderContext;

struct ImGuiDrawCmd
{
    uint32_t elemCount;
    uint32_t idxOffset;
    int32_t vtxOffset;
    float clipRect[4];
};

class ImGuiRenderer : public RenderExtension
{
public:
    ImGuiRenderer();
    ~ImGuiRenderer();

    void Setup(RenderContext* ctx, RHI::CommandBuffer* cmd) override;
    void Execute(RenderContext* ctx, RHI::CommandBuffer* cmd) override;

private:
    RHI::Texture* m_fontTexture = nullptr;
    RHI::Sampler* m_fontSampler = nullptr;

    std::vector<ImGuiDrawCmd> m_drawCmds;
    int m_drawCmdCount = 0;
    glm::mat4 m_projMatrix;
};