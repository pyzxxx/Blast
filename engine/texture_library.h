#pragma once

#include "asset_store.h"
#include "gpu_driver.h"
#include "loadset.h"
#include "render_core.h"

#include <utility>

using TextureHandle = uint64_t;

struct Services;
struct TextureData;

struct Texture {
    GpuImage image = k_gpu_invalid;
    GpuImageView view = k_gpu_invalid;
    uint32_t slot = k_invalid_slot;
};

class TextureLibrary {
public:
    bool init(uint32_t cap, const Services& services);
    void shutdown();

    const Texture& get(TextureHandle h) const { return store_.get(h); }
    bool valid(TextureHandle h) const { return store_.valid(h); }
    bool resident(TextureHandle h) const { return store_.resident(h); }
    uint32_t slot_of(TextureHandle h) const;

    void watch_commit(TextureHandle h, void* owner, AssetStore<Texture>::CommitFn fn) { store_.watch_commit(h, owner, std::move(fn)); }
    void drop_watchers(void* owner) { store_.drop_watchers(owner); }

private:
    friend class AssetManager;

    void commit(TextureHandle h, const Texture& tex);
    GpuImage upload(const TextureData& data);
    void release_ref(TextureHandle h);

    AssetStore<Texture>& store() { return store_; }

    AssetStore<Texture> store_;
    GpuDriver* gpu_ = nullptr;
    RenderCore* rcore_ = nullptr;
};
