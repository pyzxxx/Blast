#pragma once

#include "handle_pool.h"
#include "log.h"
#include "mem.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>

template<class T>
class AssetStore {
public:
    using CommitFn = std::function<void(uint64_t h, T& asset)>;

    bool init(uint32_t cap, uint8_t tag = 0) {
        if (!pool_.init(cap, tag)) {
            return false;
        }

        ids_ = static_cast<uint64_t*>(memalloc(sizeof(uint64_t) * cap));
        resident_ = static_cast<uint8_t*>(memalloc(cap));
        if (!ids_ || !resident_) {
            LOGE("AssetStore: alloc failed (cap %u)", cap);
            return false;
        }

        memset(resident_, 0, cap);
        return true;
    }

    template<class F>
    void shutdown(F&& on_free) {
        for (const auto& [id, h] : by_id_) {
            on_free(pool_.resolve(h));
        }

        by_id_.clear();
        watchers_.clear();
        pool_.shutdown();
        memfree(ids_);
        memfree(resident_);
        ids_ = nullptr;
        resident_ = nullptr;
    }

    uint64_t register_or_find(uint64_t id) {
        uint64_t h = find(id);
        if (h == k_handle_invalid) {
            h = pool_.alloc();
            if (h == k_handle_invalid) {
                return k_handle_invalid;
            }

            ids_[HandlePool<T>::index_of(h)] = id;
            by_id_.emplace(id, h);
        }

        return h;
    }

    void add_ref(uint64_t h) {
        pool_.add_ref(h);
    }

    template<class F>
    void release_ref(uint64_t h, F&& on_free) {
        if (pool_.release_ref(h)) {
            erase(h, on_free);
        }
    }

    template<class F>
    void erase(uint64_t h, F&& on_free) {
        uint32_t idx = HandlePool<T>::index_of(h);
        assert(resident_[idx] == 1);
        on_free(pool_.at(idx));
        by_id_.erase(ids_[idx]);
        pool_.free_index(idx);
        resident_[idx] = 0;
        watchers_.erase(h);
    }

    void erase_unreferenced(uint64_t h) {
        uint32_t idx = HandlePool<T>::index_of(h);
        assert(resident_[idx] == 0 || resident_[idx] == 2);
        by_id_.erase(ids_[idx]);
        pool_.free_index(idx);
        resident_[idx] = 0;
        watchers_.erase(h);
    }

    void commit(uint64_t h, const T& asset) {
        uint32_t idx = HandlePool<T>::index_of(h);
        assert(resident_[idx] == 2);
        pool_.resolve(h) = asset;
        resident_[idx] = 1;

        const auto range = watchers_.equal_range(h);
        if (range.first == range.second) {
            return;
        }

        std::vector<CommitFn> fired;
        for (auto it = range.first; it != range.second; ++it) {
            fired.push_back(it->second.fn);
        }

        watchers_.erase(range.first, range.second);
        for (const CommitFn& fn : fired) {
            fn(h, pool_.resolve(h));
        }
    }

    // fn fires once when h commits; immediately if already resident. Dropped if h is erased first.
    void watch_commit(uint64_t h, void* owner, CommitFn fn) {
        if (!pool_.valid(h)) {
            return;
        }

        if (resident(h)) {
            fn(h, pool_.resolve(h));
            return;
        }

        watchers_.emplace(h, CommitWatcher{owner, std::move(fn)});
    }

    void drop_watchers(void* owner) {
        for (auto it = watchers_.begin(); it != watchers_.end();) {
            if (it->second.owner == owner) {
                it = watchers_.erase(it);
            } else {
                ++it;
            }
        }
    }

    const T& get(uint64_t h) const { return pool_.resolve(h); }

    T& get(uint64_t h) { return pool_.resolve(h); }

    const T& get_by_index(uint32_t idx) const { return pool_.at(idx); }

    uint32_t index_of(uint64_t h) const { return HandlePool<T>::index_of(h); }

    bool resident(uint64_t h) const { return resident_[HandlePool<T>::index_of(h)] == 1; }

    bool loading(uint64_t h) const { return resident_[HandlePool<T>::index_of(h)] == 2; }

    void mark_loading(uint64_t h) {
        uint32_t idx = HandlePool<T>::index_of(h);
        assert(resident_[idx] == 0);
        resident_[idx] = 2;
    }

    bool valid(uint64_t h) const { return pool_.valid(h); }

    uint64_t find(uint64_t id) const {
        auto it = by_id_.find(id);
        return it != by_id_.end() ? it->second : k_handle_invalid;
    }

private:
    struct CommitWatcher {
        void* owner;
        CommitFn fn;
    };

    HandlePool<T> pool_;
    std::unordered_map<uint64_t, uint64_t> by_id_;
    std::unordered_multimap<uint64_t, CommitWatcher> watchers_;
    uint64_t* ids_ = nullptr;
    uint8_t* resident_ = nullptr;
};
