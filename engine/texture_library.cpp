#include "texture_library.h"
#include "render_core.h"
#include "services.h"
#include "texture_codec.h"

#include <cassert>
#include <cstring>

GpuImage TextureLibrary::upload(const TextureData& data) {
    GpuImageDesc desc = {
        .width = data.width,
        .height = data.height,
        .format = data.format,
        .mip_levels = data.mip_levels,
        .usage = GpuImageUsage::Sampled | GpuImageUsage::TransferDst,
    };
    GpuImage image = gpu_->create_image(desc);
    if (image == k_gpu_invalid) {
        return k_gpu_invalid;
    }

    const uint32_t size = static_cast<uint32_t>(data.pixels.size());
    const uint32_t offset = rcore_->upload_alloc(size);
    if (offset == UINT32_MAX) {
        rcore_->delay_delete_image(image);
        return k_gpu_invalid;
    }

    memcpy(rcore_->upload_cpu_addr() + offset, data.pixels.data(), size);

    GpuCmd cmd = rcore_->new_upload_cmd();
    gpu_->cmd_image_barrier(cmd, image, ResourceState::Undefined, ResourceState::TransferDst);
    for (uint32_t level = 0; level < data.mip_levels; ++level) {
        gpu_->cmd_copy_to_image(cmd, image, rcore_->upload_buffer(), offset + data.level_offsets[level], level);
    }

    gpu_->cmd_image_barrier(cmd, image, ResourceState::TransferDst, ResourceState::ShaderRead);
    return image;
}

bool TextureLibrary::init(uint32_t cap, const Services& services) {
    if (!store_.init(cap, static_cast<uint8_t>(HandleTag::Texture))) {
        return false;
    }

    gpu_ = services.gpu;
    rcore_ = services.rcore;
    return true;
}

void TextureLibrary::shutdown() {
    store_.shutdown([this](Texture& t) {
        if (t.image != k_gpu_invalid) {
            rcore_->delay_delete_image(t.image);
        }

        if (t.view != k_gpu_invalid) {
            rcore_->heap_remove_view(t.view);
        }

        t.slot = k_invalid_slot;
    });
}

uint32_t TextureLibrary::slot_of(TextureHandle h) const {
    if (!store_.valid(h)) {
        return k_invalid_slot;
    }

    return store_.get(h).slot;
}

void TextureLibrary::commit(TextureHandle h, const Texture& tex) {
    Texture t = tex;
    if (t.image != k_gpu_invalid) {
        if (t.view == k_gpu_invalid) {
            t.view = gpu_->create_image_view(t.image);
        }

        t.slot = rcore_->heap_add_view(t.view);
    }

    store_.commit(h, t);
}

void TextureLibrary::release_ref(TextureHandle h) {
    store_.release_ref(h, [this](Texture& t) {
        if (t.image != k_gpu_invalid) {
            rcore_->delay_delete_image(t.image);
        }

        if (t.view != k_gpu_invalid) {
            rcore_->heap_remove_view(t.view);
        }

        t.slot = k_invalid_slot;
    });
}
