#pragma once

#include "pak_format.h"

#include <cstdint>
#include <vector>

struct MeshData {
    std::vector<pak_format::VertexPos> positions;
    std::vector<pak_format::VertexAttr> attrs;
    std::vector<uint32_t> indices;
    std::vector<uint64_t> material_ids;
    float bounds_min[3] = {};
    float bounds_max[3] = {};
};

bool mesh_decode(const uint8_t* data, uint64_t size, MeshData& out);
