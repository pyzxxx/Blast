#pragma once

#include "pak_format.h"
#include "shader_interop.h"

#include <cstdint>

struct MaterialData {
    MaterialBlock block = {};
    uint64_t texture_ids[k_tex_count] = {};
};

bool material_decode(const uint8_t* data, uint64_t size, MaterialData& out);
