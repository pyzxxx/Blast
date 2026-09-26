#pragma once

#include "asset_store.h"
#include "gpu_driver.h"
#include "loadset.h"

class RenderCore;
struct Services;
struct MeshData;

using MeshHandle = uint64_t;

enum class MeshLayout : uint8_t {
    Static,
    Count
};

struct Lod {
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    uint32_t base_vertex = 0;
    float screen_error = 0;
    uint32_t resident = 0;
};

struct Mesh {
    GpuBuffer vb = k_gpu_invalid;
    GpuBuffer ib = k_gpu_invalid;
    uint32_t attr_off = 0;
    Lod lods[4] = {};
    MeshLayout layout = MeshLayout::Static;
    float bounds_min[3] = {};
    float bounds_max[3] = {};
};

class MeshLibrary {
public:
    bool init(uint32_t cap, const Services& services);
    void shutdown();

    const Mesh& get(MeshHandle h) const { return store_.get(h); }
    bool valid(MeshHandle h) const { return store_.valid(h); }
    bool resident(MeshHandle h) const { return store_.resident(h); }

private:
    friend class AssetManager;

    void commit(MeshHandle h, const Mesh& mesh);
    bool upload(const MeshData& data, Mesh& out);
    void release_ref(MeshHandle h);

    AssetStore<Mesh>& store() { return store_; }

    AssetStore<Mesh> store_;
    GpuDriver* gpu_ = nullptr;
    RenderCore* rcore_ = nullptr;
};
