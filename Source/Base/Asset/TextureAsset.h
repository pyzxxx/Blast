#pragma once

#include "Asset.h"
#include "RHI/RHI.h"

class TextureAsset : public Asset
{
public:
    TextureAsset(const std::string& assetPath);
    virtual ~TextureAsset();

    RHI::Texture* GetTexture() const { return m_texture; }

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    uint32_t GetDepth() const { return m_depth; }
    uint32_t GetLevels() const { return m_levels; }
    VkFormat GetFormat() const { return m_format; }

private:
    void LoadFromRawImage(const std::string& imagePath);

    RHI::Texture* m_texture = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_depth = 1;
    uint32_t m_levels = 1;
    VkFormat m_format = VK_FORMAT_R8G8B8A8_UNORM;
};
