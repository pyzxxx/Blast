#include "material_library.h"
#include "log.h"
#include "mem.h"
#include "render_core.h"
#include "services.h"

#include <cassert>
#include <cstring>

bool MaterialLibrary::init(uint32_t cap, const Services& services, TextureLibrary& textures) {
    if (!store_.init(cap, static_cast<uint8_t>(HandleTag::Material))) {
        return false;
    }

    gpu_ = services.gpu;
    rcore_ = services.rcore;
    textures_ = &textures;
    cap_ = cap;
    GpuBufferDesc desc = {static_cast<uint64_t>(cap) * k_material_stride,
                          GpuBufferUsage::Storage | GpuBufferUsage::TransferDst | GpuBufferUsage::DeviceAddress,
                          GpuMemoryDomain::GpuOnly};
    params_buf_ = gpu_->create_buffer(desc);
    if (params_buf_ == k_gpu_invalid) {
        return false;
    }

    params_addr_ = gpu_->gpu_address(params_buf_);
    dirty_words_ = (cap + 63) / 64;
    dirty_ = static_cast<uint64_t*>(memalloc(dirty_words_ * sizeof(uint64_t)));
    if (!dirty_) {
        LOGE("MaterialLibrary: alloc failed (cap %u)", cap);
        return false;
    }

    memset(dirty_, 0, dirty_words_ * sizeof(uint64_t));
    return true;
}

void MaterialLibrary::shutdown() {
    if (params_buf_ != k_gpu_invalid) {
        rcore_->delay_delete_buffer(params_buf_);
    }

    params_buf_ = k_gpu_invalid;
    params_addr_ = 0;
    memfree(dirty_);
    dirty_ = nullptr;
    dirty_words_ = 0;
    if (textures_ != nullptr) {
        textures_->drop_watchers(this);
    }

    store_.shutdown([](MaterialBlock&) {});
}

void MaterialLibrary::commit(MaterialHandle h, const MaterialBlock& block, std::span<const TextureHandle> textures) {
    assert(textures.size() == k_tex_count);
    MaterialBlock b = block;
    for (uint32_t i = 0; i < k_tex_count; ++i) {
        TextureHandle tex = textures[i];
        b.slot[i] = k_invalid_slot;
        if (tex == k_handle_invalid) {
            continue;
        }

        const uint32_t slot = textures_->slot_of(tex);
        if (slot != k_invalid_slot) {
            b.slot[i] = slot;
            continue;
        }

        textures_->watch_commit(tex, this, [this, h, i](uint64_t, Texture& t) {
            if (store_.valid(h)) {
                store_.get(h).slot[i] = t.slot;
                mark_dirty(store_.index_of(h));
            }
        });
    }

    store_.commit(h, b);
    mark_dirty(store_.index_of(h));
}

void MaterialLibrary::release_ref(MaterialHandle h) {
    store_.release_ref(h, [](MaterialBlock&) {});
}

void MaterialLibrary::flush() {
    GpuCmd cmd = k_gpu_invalid;
    uint32_t row = 0;
    while (row < cap_) {
        if (!(dirty_[row >> 6] & (1ull << (row & 63)))) {
            ++row;
            continue;
        }

        uint32_t first = row;
        while (row < cap_ && (dirty_[row >> 6] & (1ull << (row & 63)))) {
            dirty_[row >> 6] &= ~(1ull << (row & 63));
            ++row;
        }

        uint64_t bytes = static_cast<uint64_t>(row - first) * k_material_stride;
        if (cmd == k_gpu_invalid) {
            cmd = rcore_->new_upload_cmd();
        }

        uint32_t off = rcore_->upload_alloc(static_cast<uint32_t>(bytes));
        if (off == UINT32_MAX) {
            LOGE("MaterialLibrary::flush: upload ring exhausted");
            return;
        }

        memcpy(rcore_->upload_cpu_addr() + off, &store_.get_by_index(first), bytes);
        gpu_->cmd_copy_buffer(cmd, params_buf_, static_cast<uint64_t>(first) * k_material_stride, rcore_->upload_buffer(), off, bytes);
    }

    if (cmd != k_gpu_invalid) {
        gpu_->cmd_buffer_barrier(cmd, params_buf_, ResourceState::TransferDst, ResourceState::ShaderRead);
    }
}
