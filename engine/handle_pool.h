#pragma once

#include "log.h"
#include "mem.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <new>

constexpr uint64_t k_handle_invalid = 0xFFFFFFFFFFFFFFFF;

inline uint8_t handle_user(uint64_t h) {
    return static_cast<uint8_t>(h >> 56);
}

template<class Rec>
class HandlePool {
public:
    bool init(uint32_t cap, uint8_t user = 0) {
        recs_ = static_cast<Rec*>(memalloc(sizeof(Rec) * cap));
        generations_ = static_cast<uint32_t*>(memalloc(sizeof(uint32_t) * cap));
        free_list_ = static_cast<uint32_t*>(memalloc(sizeof(uint32_t) * cap));
        refs_ = static_cast<uint16_t*>(memalloc(sizeof(uint16_t) * cap));
        if (!recs_ || !generations_ || !free_list_ || !refs_) {
            LOGE("HandlePool: alloc failed (cap %u)", cap);
            return false;
        }

        memset(refs_, 0, sizeof(uint16_t) * cap);
        cap_ = cap;
        user_ = user;
        free_head_ = 0;
        free_tail_ = cap;
        for (uint32_t i = 0; i < cap; ++i) {
            new (&recs_[i]) Rec();
            generations_[i] = 1;
        }

        return true;
    }

    void shutdown() {
        for (uint32_t i = 0; i < cap_; ++i) {
            recs_[i].~Rec();
        }

        memfree(recs_);
        memfree(generations_);
        memfree(free_list_);
        memfree(refs_);
        recs_ = nullptr;
        generations_ = nullptr;
        free_list_ = nullptr;
        refs_ = nullptr;
        cap_ = 0;
    }

    template<class... Args>
    uint64_t alloc(Args&&... args) {
        if (free_head_ == free_tail_) {
            LOGE("HandlePool: capacity %u exhausted", cap_);
            return k_handle_invalid;
        }

        uint32_t idx = free_head_ < cap_ ? free_head_ : free_list_[free_head_ % cap_];
        free_head_++;
        recs_[idx] = Rec{args...};
        refs_[idx] = 0;
        return pack(idx);
    }

    void free(uint64_t handle) {
        if (handle == k_handle_invalid) {
            return;
        }

        assert(valid(handle));
        free_index(index_of(handle));
    }

    void free_index(uint32_t idx) {
        assert(idx < cap_);
        generations_[idx]++;
        free_list_[free_tail_ % cap_] = idx;
        free_tail_++;
    }

    Rec& resolve(uint64_t handle) {
        assert(valid(handle));
        return recs_[index_of(handle)];
    }

    const Rec& resolve(uint64_t handle) const {
        assert(valid(handle));
        return recs_[index_of(handle)];
    }

    Rec& at(uint32_t idx) {
        return recs_[idx];
    }

    const Rec& at(uint32_t idx) const {
        return recs_[idx];
    }

    void add_ref(uint64_t handle) {
        refs_[index_of(handle)]++;
    }

    bool release_ref(uint64_t handle) {
        uint16_t& refs = refs_[index_of(handle)];
        assert(refs > 0);
        return --refs == 0;
    }

    uint32_t capacity() const {
        return cap_;
    }

    bool valid(uint64_t handle) const {
        uint32_t idx = index_of(handle);
        return idx < cap_ && (generations_[idx] & k_gen_mask) == (static_cast<uint32_t>(handle >> 32) & k_gen_mask);
    }

    static uint32_t index_of(uint64_t handle) {
        return static_cast<uint32_t>(handle);
    }

    uint64_t handle_of(uint32_t idx) const {
        assert(idx < cap_);
        return pack(idx);
    }

private:
    static constexpr uint32_t k_gen_mask = 0x00FFFFFF;

    uint64_t pack(uint32_t idx) const {
        return (static_cast<uint64_t>(user_) << 56) | (static_cast<uint64_t>(generations_[idx] & k_gen_mask) << 32) | idx;
    }

    Rec* recs_ = nullptr;
    uint32_t* generations_ = nullptr;
    uint32_t* free_list_ = nullptr;
    uint16_t* refs_ = nullptr;
    uint32_t cap_ = 0;
    uint8_t user_ = 0;
    uint64_t free_head_ = 0;
    uint64_t free_tail_ = 0;
};
