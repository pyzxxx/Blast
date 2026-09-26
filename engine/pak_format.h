#pragma once

#include "shader_interop.h"

#include <cstdint>

namespace pak_format {

inline constexpr uint32_t k_package_magic = 0x424B5047u;
inline constexpr uint32_t k_manifest_magic = 0x424D414Eu;
inline constexpr uint32_t k_prefab_magic = 0x42504642u;
inline constexpr uint32_t k_mesh_magic = 0x424D4553u;
inline constexpr uint32_t k_material_magic = 0x424D4154u;

inline constexpr uint32_t k_package_version = 4u;
inline constexpr uint64_t k_footer_size = 12u;

enum class AssetType : uint8_t {
    Prefab = 0,
    Mesh = 1,
    Material = 2,
    Texture = 3,
};

struct ManifestHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t dependency_count;
    uint64_t dependency_id_table_offset;
    uint64_t path_table_offset;
};
static_assert(sizeof(ManifestHeader) == 32);

struct ManifestEntry {
    uint64_t id;
    uint64_t offset;
    uint64_t size;
    uint8_t type;
    uint8_t flags;
    uint8_t pad[6];
};
static_assert(sizeof(ManifestEntry) == 32);

struct DependencyEntry {
    uint64_t asset_id;
    uint32_t dependency_count;
    uint32_t first_dependency_index;
};
static_assert(sizeof(DependencyEntry) == 16);

struct PrefabHeader {
    uint32_t magic;
    uint32_t node_count;
    uint32_t string_table_size;
    uint32_t component_count;
};
static_assert(sizeof(PrefabHeader) == 16);

struct PrefabNode {
    uint64_t mesh_id;
    uint64_t material_id;
    uint32_t name_offset;
    int32_t parent_index;
    float position[3];
    float rotation[4];
    float scale[3];
};
static_assert(sizeof(PrefabNode) == 64);

inline constexpr uint32_t k_component_camera = 1u;
inline constexpr uint32_t k_component_light = 2u;

struct PrefabComponent {
    uint32_t node_index;
    uint32_t type;
    uint8_t payload[24];
};
static_assert(sizeof(PrefabComponent) == 32);

struct MeshHeader {
    uint32_t magic;
    uint32_t surface_count;
    float bounds_min[3];
    float bounds_max[3];
};
static_assert(sizeof(MeshHeader) == 32);

struct MeshSurfaceHeader {
    uint32_t vertex_count;
    uint32_t index_count;
    uint64_t material_id;
};
static_assert(sizeof(MeshSurfaceHeader) == 16);

struct VertexPos {
    float position[3];
};
static_assert(sizeof(VertexPos) == 12);

struct VertexAttr {
    float normal[3];
    float tangent[4];
    float uv[2];
};
static_assert(sizeof(VertexAttr) == 36);

struct MaterialHeader {
    uint32_t magic;
    uint8_t block[k_material_stride];
};
static_assert(sizeof(MaterialHeader) == 4 + k_material_stride);

inline uint64_t hash_path(const char* path) {
    uint64_t hash = 14695981039346656037ull;
    for (const char* p = path; *p != '\0'; ++p) {
        const char c = *p;
        const uint8_t lower = (c >= 'A' && c <= 'Z') ? static_cast<uint8_t>(c + ('a' - 'A')) : static_cast<uint8_t>(c);
        hash ^= lower;
        hash *= 1099511628211ull;
    }

    return hash;
}

} // namespace pak_format
