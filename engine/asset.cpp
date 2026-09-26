#include "asset.h"
#include "log.h"
#include "material_codec.h"
#include "mesh_codec.h"
#include "pak_manager.h"
#include "services.h"
#include "texture_codec.h"

#include <cassert>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace {

constexpr float k_drain_budget_ms = 8.0f;

struct LoadResult {
    TextureData texture;
    MeshData mesh;
    MaterialData material;
};

bool read_blob_worker(const std::string& pak_path, const pak_format::ManifestEntry& entry, std::vector<uint8_t>& out) {
    File file = File::open(pak_path.c_str());
    if (!file.is_open()) {
        return false;
    }

    const uint64_t file_size = file.size();
    if (entry.offset > file_size || entry.size > file_size - entry.offset) {
        return false;
    }

    out.resize(static_cast<size_t>(entry.size));
    file.seek(entry.offset);
    if (entry.size > 0 && file.read(out.data(), entry.size) != entry.size) {
        out.clear();
        return false;
    }

    return true;
}

template<class Data>
std::function<bool()> decode_work(const std::string& pak_path, const pak_format::ManifestEntry& entry,
                                  const std::shared_ptr<LoadResult>& result, Data LoadResult::* field,
                                  bool (*decode)(const uint8_t*, uint64_t, Data&)) {
    return [pak_path, entry, result, field, decode]() {
        std::vector<uint8_t> bytes;
        return read_blob_worker(pak_path, entry, bytes) && decode(bytes.data(), bytes.size(), result.get()->*field);
    };
}

JobPriority to_job_priority(AssetPriority p) {
    switch (p) {
        case AssetPriority::High:
            return JobPriority::High;
        case AssetPriority::Background:
            return JobPriority::Background;
        default:
            return JobPriority::Normal;
    }
}

AssetPriority dep_priority(AssetPriority p) {
    return p == AssetPriority::Background ? AssetPriority::Normal : p;
}

} // namespace

bool AssetManager::init(const Services& services) {
    paks_ = services.paks;
    sets_ = services.loadsets;
    jobs_ = services.jobs;

    if (!meshes_.init(k_max_meshes, services)) {
        return false;
    }

    if (!textures_.init(k_max_textures, services)) {
        return false;
    }

    if (!materials_.init(k_max_materials, services, textures_)) {
        return false;
    }

    if (!pipeline_.init(jobs_, sets_)) {
        return false;
    }

    sets_->register_hooks(HandleTag::Mesh,
                          {this, [](void* ctx, uint64_t h) { static_cast<AssetManager*>(ctx)->meshes_.store().add_ref(h); },
                           [](void* ctx, uint64_t h) { static_cast<AssetManager*>(ctx)->meshes_.release_ref(h); }});
    sets_->register_hooks(HandleTag::Material,
                          {this, [](void* ctx, uint64_t h) { static_cast<AssetManager*>(ctx)->materials_.store().add_ref(h); },
                           [](void* ctx, uint64_t h) { static_cast<AssetManager*>(ctx)->materials_.release_ref(h); }});
    sets_->register_hooks(HandleTag::Texture,
                          {this, [](void* ctx, uint64_t h) { static_cast<AssetManager*>(ctx)->textures_.store().add_ref(h); },
                           [](void* ctx, uint64_t h) { static_cast<AssetManager*>(ctx)->textures_.release_ref(h); }});
    sets_->set_release_hook({this, [](void* ctx, LoadSet set) { static_cast<AssetManager*>(ctx)->pipeline_.cancel_set(set); }});
    return true;
}

void AssetManager::shutdown() {
    pipeline_.shutdown();
    meshes_.shutdown();
    materials_.shutdown();
    textures_.shutdown();
}

void AssetManager::update() {
    drain_uploads(k_drain_budget_ms);
    materials_.flush();
}

MeshHandle AssetManager::load_mesh(const char* path, LoadSet set) {
    uint64_t id = pak_format::hash_path(path);
    return load_sync(set, id) ? meshes_.store().find(id) : k_handle_invalid;
}

MaterialHandle AssetManager::load_material(const char* path, LoadSet set) {
    uint64_t id = pak_format::hash_path(path);
    return load_sync(set, id) ? materials_.store().find(id) : k_handle_invalid;
}

TextureHandle AssetManager::load_texture(const char* path, LoadSet set) {
    uint64_t id = pak_format::hash_path(path);
    return load_sync(set, id) ? textures_.store().find(id) : k_handle_invalid;
}

MeshHandle AssetManager::load_mesh(uint64_t id, LoadSet set) {
    return load_sync(set, id) ? meshes_.store().find(id) : k_handle_invalid;
}

MaterialHandle AssetManager::load_material(uint64_t id, LoadSet set) {
    return load_sync(set, id) ? materials_.store().find(id) : k_handle_invalid;
}

bool AssetManager::prefab_blob(uint64_t id, std::vector<uint8_t>& out) {
    pak_format::ManifestEntry entry;
    Pak* pak = paks_->find(id, entry);
    if (pak == nullptr || static_cast<pak_format::AssetType>(entry.type) != pak_format::AssetType::Prefab) {
        return false;
    }

    return pak->read_blob(entry, out);
}

bool AssetManager::load_sync(LoadSet set, uint64_t id) {
    return load(id, set, AssetPriority::Immediate) != k_handle_invalid;
}

void AssetManager::drain_uploads(float budget_ms) {
    pipeline_.drain(budget_ms);
}

void AssetManager::ensure_resident(uint64_t handle) {
    pipeline_.ensure(handle);
}

uint64_t AssetManager::load(uint64_t id, LoadSet set, AssetPriority p) {
    if (set == k_loadset_invalid) {
        set = sets_->global();
    }

    assert(sets_->valid(set));
    assert((p != AssetPriority::Immediate || !sets_->in_on_idle()) && "Immediate load inside an on_idle callback");

    pak_format::ManifestEntry entry;
    Pak* pak = paks_->find(id, entry);
    if (pak == nullptr) {
        LOGE("AssetManager: asset %llu not in any pak", static_cast<uint64_t>(id));
        return k_handle_invalid;
    }

    switch (static_cast<pak_format::AssetType>(entry.type)) {
    case pak_format::AssetType::Mesh:
        return load_typed(meshes_.store(), id, set, p, *pak, entry);
    case pak_format::AssetType::Material:
        return load_typed(materials_.store(), id, set, p, *pak, entry);
    case pak_format::AssetType::Texture:
        return load_typed(textures_.store(), id, set, p, *pak, entry);
    default:
        break;
    }

    LOGE("AssetManager: asset %llu is not loadable", static_cast<uint64_t>(id));
    return k_handle_invalid;
}

void AssetManager::expand_closure(Pak& pak, uint64_t id, LoadSet set, AssetPriority dep_p) {
    uint32_t dep_count = 0;
    const uint64_t* deps = pak.dependencies(id, dep_count);
    for (uint32_t i = 0; i < dep_count; ++i) {
        load(deps[i], set, dep_p);
    }
}

template<class T>
void AssetManager::abort_load(AssetStore<T>& store, const char* what, uint64_t h, uint64_t id, LoadSet set) {
    LOGE("AssetManager: %s %llu load aborted", what, static_cast<uint64_t>(id));
    store.erase_unreferenced(h);
    sets_->note_pending(set, -1);
}

template<class T>
uint64_t AssetManager::load_typed(AssetStore<T>& store, uint64_t id, LoadSet set, AssetPriority p, Pak& pak,
                                  const pak_format::ManifestEntry& entry) {
    uint64_t h = store.register_or_find(id);
    if (h == k_handle_invalid) {
        LOGE("AssetManager: pool exhausted for asset %llu", static_cast<uint64_t>(id));
        return k_handle_invalid;
    }

    if (sets_->has_member(set, h)) {
        return h;
    }

    const auto track_resident = [this, &pak, id, set, h, p]() {
        sets_->track(set, h);
        expand_closure(pak, id, set, p);
        return h;
    };

    if (store.resident(h)) {
        return track_resident();
    }

    if (store.loading(h)) {
        if (p != AssetPriority::Immediate) {
            pipeline_.add_observer(h, set);
            expand_closure(pak, id, set, dep_priority(p));
            return h;
        }

        pipeline_.ensure(h);
        if (!store.resident(h)) {
            return k_handle_invalid;
        }

        return sets_->has_member(set, h) ? h : track_resident();
    }

    store.mark_loading(h);

    expand_closure(pak, id, set, dep_priority(p));

    auto result = std::make_shared<LoadResult>();
    const std::string pak_path = pak.path();

    LoadTask task;
    task.set = set;
    task.debug_id = h;
    task.ready = [this, id]() { return deps_resolved(id); };

    if constexpr (std::is_same_v<T, Texture>) {
        task.work = decode_work(pak_path, entry, result, &LoadResult::texture, texture_decode);
        task.finalize = [this, h, id, result](LoadSet set) {
            Texture tex;
            tex.image = textures_.upload(result->texture);
            if (tex.image == k_gpu_invalid) {
                LOGE("AssetManager: texture %llu upload failed", static_cast<uint64_t>(id));
                textures_.store().erase_unreferenced(h);
            } else {
                textures_.commit(h, tex);
                sets_->track(set, h);
            }

            sets_->note_pending(set, -1);
        };
        task.abort = [this, &store, h, id](LoadSet set) { abort_load(store, "texture", h, id, set); };
    } else if constexpr (std::is_same_v<T, Mesh>) {
        task.work = decode_work(pak_path, entry, result, &LoadResult::mesh, mesh_decode);
        task.finalize = [this, h, id, result](LoadSet set) {
            Mesh mesh;
            if (!meshes_.upload(result->mesh, mesh)) {
                LOGE("AssetManager: mesh %llu upload failed", static_cast<uint64_t>(id));
                meshes_.store().erase_unreferenced(h);
            } else {
                meshes_.commit(h, mesh);
                sets_->track(set, h);
            }

            sets_->note_pending(set, -1);
        };
        task.abort = [this, &store, h, id](LoadSet set) { abort_load(store, "mesh", h, id, set); };
    } else {
        static_assert(std::is_same_v<T, MaterialBlock>);
        task.work = decode_work(pak_path, entry, result, &LoadResult::material, material_decode);
        task.finalize = [this, h, result](LoadSet set) {
            TextureHandle textures[k_tex_count];
            for (uint32_t i = 0; i < k_tex_count; ++i) {
                textures[i] = k_handle_invalid;
                if (result->material.texture_ids[i] == 0) {
                    continue;
                }

                textures[i] = textures_.store().register_or_find(result->material.texture_ids[i]);
                if (textures[i] == k_handle_invalid) {
                    LOGE("AssetManager: texture pool exhausted for guid %llu", static_cast<uint64_t>(result->material.texture_ids[i]));
                }
            }

            materials_.commit(h, result->material.block, textures);
            sets_->track(set, h);
            sets_->note_pending(set, -1);
        };
        task.abort = [this, &store, h, id](LoadSet set) { abort_load(store, "material", h, id, set); };
    }

    sets_->note_pending(set, +1);

    if (p == AssetPriority::Immediate) {
        if (task.work()) {
            task.finalize(set);
        } else {
            task.abort(set);
        }

        return store.resident(h) ? h : k_handle_invalid;
    }

    pipeline_.submit(task, to_job_priority(p));
    return h;
}

bool AssetManager::deps_resolved(uint64_t id) {
    pak_format::ManifestEntry entry;
    Pak* pak = paks_->find(id, entry);
    if (pak == nullptr) {
        return true;
    }

    const auto undecided = [](auto& store, uint64_t dep_id) {
        const uint64_t h = store.find(dep_id);
        return h != k_handle_invalid && store.valid(h) && store.loading(h);
    };

    uint32_t dep_count = 0;
    const uint64_t* deps = pak->dependencies(id, dep_count);
    for (uint32_t i = 0; i < dep_count; ++i) {
        pak_format::ManifestEntry dep_entry;
        if (paks_->find(deps[i], dep_entry) == nullptr) {
            continue;
        }

        bool waiting = false;
        switch (static_cast<pak_format::AssetType>(dep_entry.type)) {
        case pak_format::AssetType::Mesh:
            waiting = undecided(meshes_.store(), deps[i]);
            break;
        case pak_format::AssetType::Material:
            waiting = undecided(materials_.store(), deps[i]);
            break;
        case pak_format::AssetType::Texture:
            waiting = undecided(textures_.store(), deps[i]);
            break;
        default:
            break;
        }

        if (waiting) {
            return false;
        }
    }

    return true;
}
