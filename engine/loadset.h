#pragma once

#include "handle_pool.h"

#include <cstdint>
#include <functional>
#include <vector>

using LoadSet = uint64_t;
constexpr LoadSet k_loadset_invalid = k_handle_invalid;
constexpr uint32_t k_max_loadsets = 512;

enum class HandleTag : uint8_t {
    None = 0,
    Object = 1,
    Mesh = 2,
    Material = 3,
    Texture = 4,
    LoadSet = 5,
};

constexpr uint32_t k_max_handle_tags = 8;

inline bool handle_is(uint64_t h, HandleTag tag) {
    return handle_user(h) == static_cast<uint8_t>(tag);
}

class LoadSets {
public:
    struct Hooks {
        void* ctx = nullptr;
        void (*acquire)(void* ctx, uint64_t handle) = nullptr;
        void (*release)(void* ctx, uint64_t handle) = nullptr;
    };

    bool init();
    void shutdown();

    LoadSet create(const char* debug_name);
    void release(LoadSet set);
    bool valid(LoadSet set) const;
    const char* name(LoadSet set) const;
    LoadSet global() const { return global_; }

    void track(LoadSet set, uint64_t handle);
    bool has_member(LoadSet set, uint64_t handle) const;

    void register_hooks(HandleTag tag, const Hooks& hooks);

    void note_pending(LoadSet set, int delta);
    void progress(LoadSet set, uint32_t& out_done, uint32_t& out_total) const;
    void set_on_idle(LoadSet set, std::function<void()> cb);
    bool in_on_idle() const { return on_idle_depth_ > 0; }

    struct ReleaseHook {
        void* ctx = nullptr;
        void (*fn)(void* ctx, LoadSet set) = nullptr;
    };
    void set_release_hook(const ReleaseHook& hook) { release_hook_ = hook; }

private:
    struct Slot {
        char name[32] = {};
        std::vector<uint64_t> members;
        uint32_t pending = 0;
        uint32_t total = 0;
        std::function<void()> on_idle;
    };

    void dispatch_acquire(uint64_t handle);
    void dispatch_release(uint64_t handle);

    HandlePool<Slot> sets_;
    Hooks hooks_[k_max_handle_tags] = {};
    ReleaseHook release_hook_;
    uint32_t on_idle_depth_ = 0;
    LoadSet global_ = k_loadset_invalid;
};
