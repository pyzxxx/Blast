#pragma once

#include "asset_store.h"
#include "gpu_driver.h"
#include "loadset.h"
#include "shader_interop.h"
#include "texture_library.h"

#include <span>

class RenderCore;
struct Services;

using MaterialHandle = uint64_t;

class MaterialLibrary {
public:
    bool init(uint32_t cap, const Services& services, TextureLibrary& textures);
    void shutdown();

    const MaterialBlock& get(MaterialHandle h) const { return store_.get(h); }
    uint32_t index_of(MaterialHandle h) const { return store_.index_of(h); }
    uint64_t params_addr() const { return params_addr_; }
    bool valid(MaterialHandle h) const { return store_.valid(h); }
    bool resident(MaterialHandle h) const { return store_.resident(h); }

    void flush();

private:
    friend class AssetManager;

    void commit(MaterialHandle h, const MaterialBlock& block, std::span<const TextureHandle> textures);
    void release_ref(MaterialHandle h);

    void mark_dirty(uint32_t row) { dirty_[row >> 6] |= 1ull << (row & 63); }

    AssetStore<MaterialBlock>& store() { return store_; }

    AssetStore<MaterialBlock> store_;
    GpuDriver* gpu_ = nullptr;
    RenderCore* rcore_ = nullptr;
    TextureLibrary* textures_ = nullptr;
    GpuBuffer params_buf_ = k_gpu_invalid;
    uint64_t params_addr_ = 0;
    uint64_t* dirty_ = nullptr;
    uint32_t dirty_words_ = 0;
    uint32_t cap_ = 0;
};
