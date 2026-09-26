#include "loadset.h"
#include "log.h"

#include <cassert>
#include <cstdio>

bool LoadSets::init() {
    if (!sets_.init(k_max_loadsets, static_cast<uint8_t>(HandleTag::LoadSet))) {
        return false;
    }

    global_ = create("global");
    return global_ != k_loadset_invalid;
}

void LoadSets::shutdown() {
    sets_.shutdown();
}

LoadSet LoadSets::create(const char* debug_name) {
    LoadSet s = sets_.alloc();
    if (s == k_handle_invalid) {
        return k_loadset_invalid;
    }

    Slot& slot = sets_.resolve(s);
    slot.members.clear();
    slot.pending = 0;
    slot.total = 0;
    slot.on_idle = nullptr;
    snprintf(slot.name, sizeof(slot.name), "%s", debug_name);
    return s;
}

void LoadSets::release(LoadSet set) {
    if (!sets_.valid(set)) {
        return;
    }

    if (release_hook_.fn) {
        release_hook_.fn(release_hook_.ctx, set);
    }

    Slot& slot = sets_.resolve(set);
    slot.on_idle = nullptr;
    slot.pending = 0;
    slot.total = 0;

    const std::vector<uint64_t>& members = slot.members;
    for (uint64_t h : members) {
        if (handle_is(h, HandleTag::Object)) {
            dispatch_release(h);
        }
    }

    for (uint64_t h : members) {
        if (!handle_is(h, HandleTag::Object)) {
            dispatch_release(h);
        }
    }

    sets_.free(set);
}

bool LoadSets::valid(LoadSet set) const {
    return sets_.valid(set);
}

const char* LoadSets::name(LoadSet set) const {
    assert(sets_.valid(set));
    return sets_.resolve(set).name;
}

bool LoadSets::has_member(LoadSet set, uint64_t handle) const {
    assert(sets_.valid(set));
    for (uint64_t h : sets_.resolve(set).members) {
        if (h == handle) {
            return true;
        }
    }

    return false;
}

void LoadSets::track(LoadSet set, uint64_t handle) {
    assert(sets_.valid(set));
    assert(!has_member(set, handle));
    sets_.resolve(set).members.push_back(handle);
    dispatch_acquire(handle);
}

void LoadSets::register_hooks(HandleTag tag, const Hooks& hooks) {
    uint8_t idx = static_cast<uint8_t>(tag);
    assert(idx < k_max_handle_tags);
    hooks_[idx] = hooks;
}

void LoadSets::note_pending(LoadSet set, int delta) {
    if (!sets_.valid(set)) {
        return;
    }

    Slot& slot = sets_.resolve(set);
    if (delta > 0) {
        slot.pending += static_cast<uint32_t>(delta);
        slot.total += static_cast<uint32_t>(delta);
        return;
    }

    const uint32_t dec = static_cast<uint32_t>(-delta);
    assert(slot.pending >= dec);
    slot.pending -= dec;
    if (slot.pending == 0 && slot.on_idle) {
        std::function<void()> cb = std::move(slot.on_idle);
        slot.on_idle = nullptr;
        ++on_idle_depth_;
        cb();
        --on_idle_depth_;
    }
}

void LoadSets::progress(LoadSet set, uint32_t& out_done, uint32_t& out_total) const {
    assert(sets_.valid(set));
    const Slot& slot = sets_.resolve(set);
    out_total = slot.total;
    out_done = slot.total - slot.pending;
}

void LoadSets::set_on_idle(LoadSet set, std::function<void()> cb) {
    assert(sets_.valid(set));
    sets_.resolve(set).on_idle = std::move(cb);
}

void LoadSets::dispatch_acquire(uint64_t handle) {
    const Hooks& hooks = hooks_[handle_user(handle)];
    if (hooks.acquire) {
        hooks.acquire(hooks.ctx, handle);
    }
}

void LoadSets::dispatch_release(uint64_t handle) {
    const Hooks& hooks = hooks_[handle_user(handle)];
    if (hooks.release) {
        hooks.release(hooks.ctx, handle);
    }
}
