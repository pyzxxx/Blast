#include "mesh_codec.h"

#include <algorithm>
#include <cstring>

bool mesh_decode(const uint8_t* data, uint64_t size, MeshData& out) {
    if (size < sizeof(pak_format::MeshHeader)) {
        return false;
    }

    pak_format::MeshHeader hdr;
    memcpy(&hdr, data, sizeof(hdr));
    if (hdr.magic != pak_format::k_mesh_magic) {
        return false;
    }

    const uint8_t* p = data + sizeof(pak_format::MeshHeader);
    const uint8_t* end = data + size;
    for (uint32_t s = 0; s < hdr.surface_count; ++s) {
        if (p + sizeof(pak_format::MeshSurfaceHeader) > end) {
            return false;
        }

        pak_format::MeshSurfaceHeader sh;
        memcpy(&sh, p, sizeof(sh));
        p += sizeof(sh);

        uint64_t pos_bytes = static_cast<uint64_t>(sh.vertex_count) * sizeof(pak_format::VertexPos);
        uint64_t attr_bytes = static_cast<uint64_t>(sh.vertex_count) * sizeof(pak_format::VertexAttr);
        uint64_t ib_bytes = static_cast<uint64_t>(sh.index_count) * sizeof(uint32_t);
        if (p + pos_bytes + attr_bytes + ib_bytes > end) {
            return false;
        }

        uint32_t base = static_cast<uint32_t>(out.positions.size());
        const pak_format::VertexPos* src_pos = reinterpret_cast<const pak_format::VertexPos*>(p);
        out.positions.insert(out.positions.end(), src_pos, src_pos + sh.vertex_count);
        p += pos_bytes;

        const pak_format::VertexAttr* src_attr = reinterpret_cast<const pak_format::VertexAttr*>(p);
        out.attrs.insert(out.attrs.end(), src_attr, src_attr + sh.vertex_count);
        p += attr_bytes;

        const uint32_t* src_idx = reinterpret_cast<const uint32_t*>(p);
        for (uint32_t i = 0; i < sh.index_count; ++i) {
            out.indices.push_back(src_idx[i] + base);
        }

        p += ib_bytes;

        if (sh.material_id != 0 &&
            std::find(out.material_ids.begin(), out.material_ids.end(), sh.material_id) == out.material_ids.end()) {
            out.material_ids.push_back(sh.material_id);
        }
    }

    if (out.positions.empty() || out.indices.empty()) {
        return false;
    }

    memcpy(out.bounds_min, hdr.bounds_min, sizeof(out.bounds_min));
    memcpy(out.bounds_max, hdr.bounds_max, sizeof(out.bounds_max));
    return true;
}
