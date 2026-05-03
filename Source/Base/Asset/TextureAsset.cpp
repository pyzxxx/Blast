#include "TextureAsset.h"
#include "Foundation/FileSystem.h"
#include "Foundation/JsonIO.h"
#include "Foundation/VFS.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TextureAsset::TextureAsset(const std::string& assetPath) : Asset(assetPath)
{
    JsonReader reader(assetPath);

    std::string binaryPath;
    reader.Field("binary", binaryPath);

    // Load image file and decode
    std::shared_ptr<FS::File> file = std::shared_ptr<FS::File>(VFS::Open(binaryPath, FS::FileMode::Read));
    if (!file)
    {
        LOGE("Failed to open texture: %s", binaryPath.c_str());
        return;
    }

    std::vector<uint8_t> fileData(file->GetSize());
    file->Read(fileData.data(), fileData.size());

    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &width, &height, &channels, 4);
    if (!pixels)
    {
        LOGE("Failed to decode image: %s", binaryPath.c_str());
        return;
    }

    m_width = width;
    m_height = height;
    m_depth = 1;
    m_levels = 1;
    m_format = VK_FORMAT_R8G8B8A8_UNORM;

    RHI::TextureDesc desc = {};
    desc.width = m_width;
    desc.height = m_height;
    desc.depth = m_depth;
    desc.levels = m_levels;
    desc.format = m_format;
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    RHI::CreateTexture(desc, m_texture);
    RHI::CreateBindless(m_texture);

    RHI::CommandBuffer* cmd = RHI::RequestCommandBuffer();

    RHI::ImageBarrier barrier = {};
    barrier.texture = m_texture;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    barrier.srcAccess = 0;
    barrier.dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
    RHI::CmdPipelineBarrier(cmd, barrier);

    RHI::TextureRegion region = {};
    region.width = m_width;
    region.height = m_height;
    region.depth = m_depth;
    RHI::CmdUploadTexture(cmd, m_texture, region, pixels);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier.dstAccess = VK_ACCESS_SHADER_READ_BIT;
    RHI::CmdPipelineBarrier(cmd, barrier);

    RHI::Submit(cmd);

    stbi_image_free(pixels);
}

TextureAsset::~TextureAsset()
{
    if (m_texture)
    {
        RHI::DestroyTexture(m_texture);
    }
}
