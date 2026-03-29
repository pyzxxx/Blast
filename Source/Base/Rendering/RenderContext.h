#pragma once

#include "RHI/RHI.h"

class RenderContext
{
public:
    RenderContext();
    ~RenderContext();

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    RHI::Buffer* GetBuffer(const std::string& name);
    RHI::Buffer* CreateBuffer(const std::string& name, const RHI::BufferDesc& desc, std::function<void(RHI::Buffer*)> initCallback = nullptr);

    RHI::Texture* GetTexture(const std::string& name);
    RHI::Texture* CreateTexture(const std::string& name, const RHI::TextureDesc& desc, std::function<void(RHI::Texture*)> initCallback = nullptr);

    RHI::Pipeline* GetGraphicsPipeline(const std::string& name);
    RHI::Pipeline* CreateGraphicsPipeline(const std::string& name, const RHI::GraphicsPipelineDesc& desc);

    RHI::Pipeline* GetComputePipeline(const std::string& name);
    RHI::Pipeline* CreateComputePipeline(const std::string& name, const RHI::ComputePipelineDesc& desc);

    RHI::RenderPass* GetRenderPass(const std::string& name);
    RHI::RenderPass* CreateRenderPass(const std::string& name, const RHI::RenderPassDesc& desc);

    RHI::Sampler* GetSampler(const std::string& name) const;

private:
    void InitializeSamplers();
    friend class Renderer;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    std::unordered_map<std::string, RHI::Buffer*> m_bufferCache;
    std::unordered_map<std::string, RHI::BufferDesc> m_bufferDescCache;
    std::unordered_map<std::string, RHI::Texture*> m_textureCache;
    std::unordered_map<std::string, RHI::TextureDesc> m_textureDescCache;

    std::unordered_map<std::string, RHI::Pipeline*> m_graphicsPipelineCache;
    std::unordered_map<std::string, RHI::GraphicsPipelineDesc> m_graphicsPipelineDescCache;
    std::unordered_map<std::string, RHI::Pipeline*> m_computePipelineCache;
    std::unordered_map<std::string, RHI::ComputePipelineDesc> m_computePipelineDescCache;

    std::unordered_map<std::string, RHI::RenderPass*> m_renderPassCache;
    std::unordered_map<std::string, RHI::RenderPassDesc> m_renderPassDescCache;

    std::unordered_map<std::string, RHI::Sampler*> m_samplerCache;
};
