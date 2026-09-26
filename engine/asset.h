#pragma once

#include "asset_store.h"
#include "handle_pool.h"
#include "load_pipeline.h"
#include "loadset.h"
#include "material_library.h"
#include "mesh_library.h"
#include "pak.h"
#include "texture_library.h"

#include <cstdint>
#include <vector>

class GpuDriver;
class JobSystem;
class PakManager;
class RenderCore;
class Vfs;
struct Services;

enum class AssetPriority : uint8_t { Immediate, High, Normal, Background };

class AssetManager {
public:
    bool init(const Services& services);
    void shutdown();
    void update();

    uint64_t load(uint64_t id, LoadSet set, AssetPriority p);

    void ensure_resident(uint64_t handle);
    void drain_uploads(float budget_ms);

    MeshHandle load_mesh(const char* path, LoadSet set = k_loadset_invalid);
    MaterialHandle load_material(const char* path, LoadSet set = k_loadset_invalid);
    TextureHandle load_texture(const char* path, LoadSet set = k_loadset_invalid);

    MeshHandle load_mesh(uint64_t id, LoadSet set);
    MaterialHandle load_material(uint64_t id, LoadSet set);

    bool prefab_blob(uint64_t id, std::vector<uint8_t>& out);

    bool load_sync(LoadSet set, uint64_t id);

    const MeshLibrary& meshes() const { return meshes_; }
    const MaterialLibrary& materials() const { return materials_; }
    const TextureLibrary& textures() const { return textures_; }

    MeshLibrary& meshes() { return meshes_; }
    MaterialLibrary& materials() { return materials_; }
    TextureLibrary& textures() { return textures_; }

private:
    static constexpr uint32_t k_max_meshes = 1024;
    static constexpr uint32_t k_max_materials = 1024;
    static constexpr uint32_t k_max_textures = 2048;

    template<class T>
    uint64_t load_typed(AssetStore<T>& store, uint64_t id, LoadSet set, AssetPriority p, Pak& pak,
                        const pak_format::ManifestEntry& entry);

    void expand_closure(Pak& pak, uint64_t id, LoadSet set, AssetPriority dep_p);

    template<class T>
    void abort_load(AssetStore<T>& store, const char* what, uint64_t h, uint64_t id, LoadSet set);

    bool deps_resolved(uint64_t id);

    PakManager* paks_ = nullptr;
    LoadSets* sets_ = nullptr;
    JobSystem* jobs_ = nullptr;

    MeshLibrary meshes_;
    MaterialLibrary materials_;
    TextureLibrary textures_;

    LoadPipeline pipeline_;
};
