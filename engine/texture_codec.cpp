#include "texture_codec.h"
#include "log.h"

#include <ktx.h>

#include <cstring>
#include <mutex>

namespace {

constexpr uint32_t k_format_r8_unorm = 9;
constexpr uint32_t k_format_r8g8b8a8_unorm = 37;
constexpr uint32_t k_format_r8g8b8a8_srgb = 43;
constexpr uint32_t k_format_b8g8r8a8_unorm = 44;
constexpr uint32_t k_format_b8g8r8a8_srgb = 50;
constexpr uint32_t k_format_r16g16b16a16_sfloat = 97;
constexpr uint32_t k_format_r32g32b32a32_sfloat = 109;
constexpr uint32_t k_format_bc1_rgba_unorm = 133;
constexpr uint32_t k_format_bc2_unorm = 135;
constexpr uint32_t k_format_bc3_unorm = 137;
constexpr uint32_t k_format_bc4_unorm = 139;
constexpr uint32_t k_format_bc5_unorm = 141;
constexpr uint32_t k_format_bc6h_ufloat = 143;
constexpr uint32_t k_format_bc6h_sfloat = 144;
constexpr uint32_t k_format_bc7_unorm = 145;
constexpr uint32_t k_format_bc7_srgb = 146;

GpuFormat gpu_format_from_ktx(uint32_t vk) {
    switch (vk) {
        case k_format_r8_unorm:
            return GpuFormat::R8Unorm;
        case k_format_r8g8b8a8_unorm:
        case k_format_r8g8b8a8_srgb:
            return GpuFormat::R8G8B8A8Unorm;
        case k_format_b8g8r8a8_unorm:
        case k_format_b8g8r8a8_srgb:
            return GpuFormat::B8G8R8A8Unorm;
        case k_format_r16g16b16a16_sfloat:
            return GpuFormat::R16G16B16A16Float;
        case k_format_r32g32b32a32_sfloat:
            return GpuFormat::R32G32B32A32Float;
        case k_format_bc1_rgba_unorm:
            return GpuFormat::BC1RgbaUnorm;
        case k_format_bc2_unorm:
            return GpuFormat::BC2Unorm;
        case k_format_bc3_unorm:
            return GpuFormat::BC3Unorm;
        case k_format_bc4_unorm:
            return GpuFormat::BC4Unorm;
        case k_format_bc5_unorm:
            return GpuFormat::BC5Unorm;
        case k_format_bc6h_ufloat:
            return GpuFormat::BC6HUfloat;
        case k_format_bc6h_sfloat:
            return GpuFormat::BC6HSfloat;
        case k_format_bc7_unorm:
            return GpuFormat::BC7Unorm;
        case k_format_bc7_srgb:
            return GpuFormat::BC7Srgb;
        default:
            return GpuFormat::Undefined;
    }
}

} // namespace

bool texture_decode(const uint8_t* data, uint64_t size, TextureData& out) {
    ktxTexture2* ktex = nullptr;
    KTX_error_code err = ktxTexture2_CreateFromMemory(data, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktex);
    if (err != KTX_SUCCESS) {
        LOGE("texture_codec: failed to parse KTX2 (error %d)", err);
        return false;
    }

    if (ktxTexture2_NeedsTranscoding(ktex)) {
        static std::mutex transcode_mutex;
        std::lock_guard<std::mutex> lock(transcode_mutex);
        err = ktxTexture2_TranscodeBasis(ktex, KTX_TTF_BC7_RGBA, 0);
        if (err != KTX_SUCCESS) {
            LOGE("texture_codec: failed to transcode basis (error %d)", err);
            ktxTexture_Destroy(ktxTexture(ktex));
            return false;
        }
    }

    if (ktex->numDimensions != 2 || ktex->isArray || ktex->isCubemap) {
        LOGE("texture_codec: only 2D textures are supported");
        ktxTexture_Destroy(ktxTexture(ktex));
        return false;
    }

    const GpuFormat format = gpu_format_from_ktx(ktex->vkFormat);
    if (format == GpuFormat::Undefined) {
        LOGE("texture_codec: unsupported format %u", ktex->vkFormat);
        ktxTexture_Destroy(ktxTexture(ktex));
        return false;
    }

    out.format = format;
    out.width = ktex->baseWidth;
    out.height = ktex->baseHeight;
    out.mip_levels = ktex->numLevels;
    out.pixels.assign(ktex->pData, ktex->pData + ktex->dataSize);
    out.level_offsets.resize(ktex->numLevels);
    for (uint32_t level = 0; level < ktex->numLevels; ++level) {
        ktx_size_t offset = 0;
        if (ktxTexture_GetImageOffset(ktxTexture(ktex), level, 0, 0, &offset) != KTX_SUCCESS) {
            ktxTexture_Destroy(ktxTexture(ktex));
            return false;
        }

        out.level_offsets[level] = offset;
    }

    ktxTexture_Destroy(ktxTexture(ktex));
    return true;
}
