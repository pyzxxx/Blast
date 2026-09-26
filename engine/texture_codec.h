#pragma once

#include "gpu_driver.h"

#include <cstdint>
#include <vector>

struct TextureData {
    GpuFormat format = GpuFormat::Undefined;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mip_levels = 0;
    std::vector<uint8_t> pixels;
    std::vector<uint64_t> level_offsets;
};

bool texture_decode(const uint8_t* data, uint64_t size, TextureData& out);
