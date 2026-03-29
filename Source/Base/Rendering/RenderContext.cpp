#include "RenderContext.h"
#include <cstring>

RenderContext::RenderContext()
{
    InitializeSamplers();
}

RenderContext::~RenderContext()
{
    for (auto& iter : m_bufferCache)
    {
        RHI::DestroyBuffer(iter.second);
    }
    m_bufferCache.clear();
    m_bufferDescCache.clear();

    for (auto& iter : m_textureCache)
    {
        RHI::DestroyTexture(iter.second);
    }
    m_textureCache.clear();
    m_textureDescCache.clear();

    for (auto& iter : m_graphicsPipelineCache)
    {
        RHI::DestroyPipeline(iter.second);
    }
    m_graphicsPipelineCache.clear();
    m_graphicsPipelineDescCache.clear();

    for (auto& iter : m_computePipelineCache)
    {
        RHI::DestroyPipeline(iter.second);
    }
    m_computePipelineCache.clear();
    m_computePipelineDescCache.clear();

    for (auto& iter : m_renderPassCache)
    {
        RHI::DestroyRenderPass(iter.second);
    }
    m_renderPassCache.clear();
    m_renderPassDescCache.clear();

    for (auto& iter : m_samplerCache)
    {
        RHI::DestroySampler(iter.second);
    }
    m_samplerCache.clear();
}

RHI::Buffer* RenderContext::GetBuffer(const std::string& name)
{
    auto iter = m_bufferCache.find(name);
    if (iter != m_bufferCache.end())
    {
        return iter->second;
    }
    return nullptr;
}

RHI::Buffer* RenderContext::CreateBuffer(const std::string& name, const RHI::BufferDesc& desc, std::function<void(RHI::Buffer*)> initCallback)
{
    auto compareBufferDesc = [](const RHI::BufferDesc& a, const RHI::BufferDesc& b) -> bool {
        return a.size == b.size &&
            a.dynamicBuffer == b.dynamicBuffer &&
            a.memoryUsage == b.memoryUsage &&
            a.bufferUsage == b.bufferUsage;
    };

    RHI::Buffer* buffer = GetBuffer(name);
    if (buffer == nullptr || !compareBufferDesc(desc, m_bufferDescCache[name]))
    {
        if (buffer)
        {
            RHI::DestroyBuffer(buffer);
        }

        RHI::CreateBuffer(desc, buffer);
        m_bufferCache[name] = buffer;
        m_bufferDescCache[name] = desc;

        if (initCallback)
        {
            initCallback(buffer);
        }
    }
    return buffer;
}

RHI::Texture* RenderContext::GetTexture(const std::string& name)
{
    auto iter = m_textureCache.find(name);
    if (iter != m_textureCache.end())
    {
        return iter->second;
    }
    return nullptr;
}

RHI::Texture* RenderContext::CreateTexture(const std::string& name, const RHI::TextureDesc& desc, std::function<void(RHI::Texture*)> initCallback)
{
    auto compareTextureDesc = [](const RHI::TextureDesc& a, const RHI::TextureDesc& b) -> bool {
        return a.width == b.width &&
            a.height == b.height &&
            a.depth == b.depth &&
            a.levels == b.levels &&
            a.layers == b.layers &&
            a.format == b.format &&
            a.imageType == b.imageType &&
            a.usage == b.usage &&
            a.samples == b.samples;
    };

    RHI::Texture* texture = GetTexture(name);
    if (texture == nullptr || !compareTextureDesc(desc, m_textureDescCache[name]))
    {
        if (texture)
        {
            RHI::DestroyTexture(texture);
        }

        RHI::CreateTexture(desc, texture);
        m_textureCache[name] = texture;
        m_textureDescCache[name] = desc;

        if (initCallback)
        {
            initCallback(texture);
        }
    }
    return texture;
}

RHI::Pipeline* RenderContext::GetGraphicsPipeline(const std::string& name)
{
    auto iter = m_graphicsPipelineCache.find(name);
    if (iter != m_graphicsPipelineCache.end())
    {
        return iter->second;
    }
    return nullptr;
}

RHI::Pipeline* RenderContext::CreateGraphicsPipeline(const std::string& name, const RHI::GraphicsPipelineDesc& desc)
{
    auto compareGraphicsPipelineDesc = [](const RHI::GraphicsPipelineDesc& a, const RHI::GraphicsPipelineDesc& b) -> bool {
        if (a.vertexShader != b.vertexShader ||
            a.fragmentShader != b.fragmentShader ||
            a.renderPass != b.renderPass)
        {
            return false;
        }

        for (uint32_t i = 0; i < RHI_MAX_VERTEX_BUFFER_COUNT; ++i)
        {
            if (a.vertexLayout.bindings[i].stride != b.vertexLayout.bindings[i].stride ||
                a.vertexLayout.bindings[i].inputRate != b.vertexLayout.bindings[i].inputRate)
            {
                return false;
            }
        }
        for (uint32_t i = 0; i < RHI_MAX_VERTEX_ATTRIB_COUNT; ++i)
        {
            if (a.vertexLayout.attributes[i].binding != b.vertexLayout.attributes[i].binding ||
                a.vertexLayout.attributes[i].offset != b.vertexLayout.attributes[i].offset ||
                a.vertexLayout.attributes[i].format != b.vertexLayout.attributes[i].format)
            {
                return false;
            }
        }

        if (a.blendState.blendEnable != b.blendState.blendEnable ||
            a.blendState.srcColor != b.blendState.srcColor ||
            a.blendState.dstColor != b.blendState.dstColor ||
            a.blendState.colorOp != b.blendState.colorOp ||
            a.blendState.srcAlpha != b.blendState.srcAlpha ||
            a.blendState.dstAlpha != b.blendState.dstAlpha ||
            a.blendState.alphaOp != b.blendState.alphaOp)
        {
            return false;
        }

        if (a.depthState.depthTest != b.depthState.depthTest ||
            a.depthState.depthWrite != b.depthState.depthWrite ||
            a.depthState.depthFunc != b.depthState.depthFunc)
        {
            return false;
        }

        if (a.stencilState.stencilTest != b.stencilState.stencilTest ||
            a.stencilState.stencilReadMask != b.stencilState.stencilReadMask ||
            a.stencilState.stencilWriteMask != b.stencilState.stencilWriteMask ||
            a.stencilState.frontStencilFailOp != b.stencilState.frontStencilFailOp ||
            a.stencilState.frontStencilDepthFailOp != b.stencilState.frontStencilDepthFailOp ||
            a.stencilState.frontStencilPassOp != b.stencilState.frontStencilPassOp ||
            a.stencilState.frontStencilFunc != b.stencilState.frontStencilFunc ||
            a.stencilState.backStencilFailOp != b.stencilState.backStencilFailOp ||
            a.stencilState.backStencilDepthFailOp != b.stencilState.backStencilDepthFailOp ||
            a.stencilState.backStencilPassOp != b.stencilState.backStencilPassOp ||
            a.stencilState.backStencilFunc != b.stencilState.backStencilFunc)
        {
            return false;
        }

        if (a.multisampleState.sampleShading != b.multisampleState.sampleShading ||
            a.multisampleState.alphaToCoverage != b.multisampleState.alphaToCoverage ||
            a.multisampleState.alphaToOne != b.multisampleState.alphaToOne ||
            a.multisampleState.samples != b.multisampleState.samples)
        {
            return false;
        }

        if (a.fillMode != b.fillMode ||
            a.topology != b.topology ||
            a.frontFace != b.frontFace ||
            a.cullMode != b.cullMode)
        {
            return false;
        }

        return true;
    };

    RHI::Pipeline* pipeline = GetGraphicsPipeline(name);
    if (pipeline == nullptr || !compareGraphicsPipelineDesc(desc, m_graphicsPipelineDescCache[name]))
    {
        if (pipeline)
        {
            RHI::DestroyPipeline(pipeline);
        }

        RHI::CreateGraphicsPipeline(desc, pipeline);
        m_graphicsPipelineCache[name] = pipeline;
        m_graphicsPipelineDescCache[name] = desc;
    }
    return pipeline;
}

RHI::Pipeline* RenderContext::GetComputePipeline(const std::string& name)
{
    auto iter = m_computePipelineCache.find(name);
    if (iter != m_computePipelineCache.end())
    {
        return iter->second;
    }
    return nullptr;
}

RHI::Pipeline* RenderContext::CreateComputePipeline(const std::string& name, const RHI::ComputePipelineDesc& desc)
{
    auto compareComputePipelineDesc = [](const RHI::ComputePipelineDesc& a, const RHI::ComputePipelineDesc& b) -> bool {
        return a.computeShader == b.computeShader;
    };

    RHI::Pipeline* pipeline = GetComputePipeline(name);
    if (pipeline == nullptr || !compareComputePipelineDesc(desc, m_computePipelineDescCache[name]))
    {
        if (pipeline)
        {
            RHI::DestroyPipeline(pipeline);
        }

        RHI::CreateComputePipeline(desc, pipeline);
        m_computePipelineCache[name] = pipeline;
        m_computePipelineDescCache[name] = desc;
    }
    return pipeline;
}

RHI::RenderPass* RenderContext::GetRenderPass(const std::string& name)
{
    auto iter = m_renderPassCache.find(name);
    if (iter != m_renderPassCache.end())
    {
        return iter->second;
    }
    return nullptr;
}

RHI::RenderPass* RenderContext::CreateRenderPass(const std::string& name, const RHI::RenderPassDesc& desc)
{
    auto compareAttachmentDesc = [](const RHI::RenderPassAttachmentDesc& a, const RHI::RenderPassAttachmentDesc& b) -> bool {
        return a.texture == b.texture &&
               a.textureView == b.textureView &&
               a.resolveTexture == b.resolveTexture &&
               a.resolveTextureView == b.resolveTextureView &&
               a.resolveMode == b.resolveMode &&
               a.loadOp == b.loadOp &&
               a.storeOp == b.storeOp;
    };

    auto compareRenderPassDesc = [&](const RHI::RenderPassDesc& a, const RHI::RenderPassDesc& b) -> bool {
        if (!compareAttachmentDesc(a.depth, b.depth))
            return false;

        if (!compareAttachmentDesc(a.stencil, b.stencil))
            return false;

        for (uint32_t i = 0; i < RHI_MAX_ATTACHMENT_COUNT; ++i)
        {
            if (!compareAttachmentDesc(a.colors[i], b.colors[i]))
                return false;
        }

        return true;
    };

    RHI::RenderPass* renderPass = GetRenderPass(name);
    if (renderPass == nullptr || !compareRenderPassDesc(desc, m_renderPassDescCache[name]))
    {
        if (renderPass)
        {
            RHI::DestroyRenderPass(renderPass);
        }

        RHI::CreateRenderPass(desc, renderPass);
        m_renderPassCache[name] = renderPass;
        m_renderPassDescCache[name] = desc;
    }
    return renderPass;
}

void RenderContext::InitializeSamplers()
{
    auto createSampler = [&](const std::string& name, VkFilter filter, VkSamplerAddressMode addressMode) {
        RHI::EzSamplerDesc desc = {};
        desc.magFilter = filter;
        desc.minFilter = filter;
        desc.addressU = addressMode;
        desc.addressV = addressMode;
        desc.addressW = addressMode;
        RHI::Sampler* sampler = nullptr;
        RHI::CreateSampler(desc, sampler);
        m_samplerCache[name] = sampler;
    };

    createSampler("linearWrap", VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    createSampler("linearClamp", VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    createSampler("nearestWrap", VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    createSampler("nearestClamp", VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
}

RHI::Sampler* RenderContext::GetSampler(const std::string& name) const
{
    auto iter = m_samplerCache.find(name);
    if (iter != m_samplerCache.end())
    {
        return iter->second;
    }
    return nullptr;
}
