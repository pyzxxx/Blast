#include "render_core.h"
#include "log.h"
#include "mem.h"
#include "os.h"
#include "services.h"

#include <algorithm>

bool RenderCore::Ring::init(GpuDriver& gpu, uint32_t ring_cap, GpuBufferUsage usage) {
    GpuBufferDesc desc = {.size = ring_cap, .usage = usage, .memory = GpuMemoryDomain::Upload};
    buffer = gpu.create_buffer(desc);
    if (buffer == k_gpu_invalid) {
        return false;
    }

    map = static_cast<uint8_t*>(gpu.buffer_mapped(buffer));
    addr = gpu.gpu_address(buffer);
    if (map == nullptr) {
        return false;
    }

    cap = ring_cap;
    return true;
}

void RenderCore::Ring::shutdown(GpuDriver& gpu) {
    gpu.destroy_buffer(buffer);
}

uint32_t RenderCore::Ring::alloc(uint32_t size) {
    if (size > cap) {
        return UINT32_MAX;
    }

    uint32_t aligned = (size + k_ring_align - 1) & ~(k_ring_align - 1);
    uint32_t slot_head = head;
    uint32_t used = aligned;
    if (slot_head + aligned > cap) {
        used += cap - slot_head;
        slot_head = 0;
    }

    if (used > budget) {
        return UINT32_MAX;
    }

    head = slot_head + aligned;
    budget -= used;
    total += used;
    return slot_head;
}

void RenderCore::Ring::reclaim(uint64_t point) {
    budget = point + cap - total;
}

bool RenderCore::init(const Services& services) {
    gpu_ = services.gpu;
    OS* os = services.os;
    os->window_size(width_, height_);
    pending_width_ = width_;
    pending_height_ = height_;

    queue_ = gpu_->create_queue();
    swapchain_ = gpu_->create_swapchain(os->native_handle(), width_, height_);
    if (swapchain_ == k_gpu_invalid) {
        return false;
    }

    uint32_t image_count = gpu_->swapchain_image_count(swapchain_);
    frames_in_flight_ = std::min(image_count, k_max_frames_in_flight);

    for (auto& f : frames_) {
        f.pool = gpu_->create_cmd_pool();
        if (f.pool == k_gpu_invalid) {
            return false;
        }
    }

    for (auto& s : upload_slots_) {
        s.pool = gpu_->create_cmd_pool();
        if (s.pool == k_gpu_invalid) {
            return false;
        }
    }

    frame_sem_ = gpu_->create_semaphore(0);
    upload_sem_ = gpu_->create_semaphore(0);
    if (frame_sem_ == k_gpu_invalid || upload_sem_ == k_gpu_invalid) {
        return false;
    }

    if (!frame_ring_.init(*gpu_, k_frame_ring_size,
                          GpuBufferUsage::Storage | GpuBufferUsage::DeviceAddress | GpuBufferUsage::Vertex | GpuBufferUsage::Index)) {
        return false;
    }

    if (frame_ring_.addr == 0) {
        return false;
    }

    if (!upload_ring_.init(*gpu_, k_upload_ring_size, GpuBufferUsage::TransferSrc)) {
        return false;
    }

    GpuLayoutBinding resource_bindings[] = {
        {.binding = 0, .type = GpuDescriptorType::SampledImage, .count = k_heap_descriptor_count},
        {.binding = 1, .type = GpuDescriptorType::Sampler, .count = k_sampler_count},
    };
    resource_layout_ = gpu_->create_set_layout(resource_bindings);
    if (resource_layout_ == k_gpu_invalid) {
        return false;
    }

    resource_set_ = gpu_->create_descriptor_set(resource_layout_);
    if (resource_set_ == k_gpu_invalid) {
        return false;
    }

    default_sampler_ = gpu_->create_sampler(GpuSamplerDesc{});
    if (default_sampler_ == k_gpu_invalid) {
        return false;
    }

    gpu_->write_descriptor_sampler(resource_set_, 1, 0, default_sampler_);

    free_resource_slots_ = static_cast<uint32_t*>(memalloc(sizeof(uint32_t) * k_heap_descriptor_count));
    if (!free_resource_slots_) {
        return false;
    }

    for (uint32_t i = 0; i < k_heap_descriptor_count; ++i) {
        free_resource_slots_[i] = k_heap_descriptor_count - 1 - i;
    }

    free_resource_top_ = k_heap_descriptor_count;

    frame_cmds_.reserve(16);

    return true;
}

void RenderCore::shutdown() {
    gpu_->wait_idle();
    process_delay_deletes(true);
    gpu_->destroy_descriptor_set(resource_set_);
    gpu_->destroy_sampler(default_sampler_);
    gpu_->destroy_set_layout(resource_layout_);
    frame_ring_.shutdown(*gpu_);
    upload_ring_.shutdown(*gpu_);
    memfree(free_resource_slots_);
    free_resource_slots_ = nullptr;
    free_resource_top_ = 0;
    for (auto& s : upload_slots_) {
        gpu_->destroy_cmd_pool(s.pool);
    }

    for (auto& f : frames_) {
        gpu_->destroy_cmd_pool(f.pool);
    }

    gpu_->destroy_semaphore(upload_sem_);
    gpu_->destroy_semaphore(frame_sem_);
    gpu_->destroy_swapchain(swapchain_);
    gpu_->destroy_queue(queue_);
}

void RenderCore::resize(uint32_t width, uint32_t height) {
    pending_width_ = width;
    pending_height_ = height;
}

bool RenderCore::begin_frame() {
    if (pending_width_ != width_ || pending_height_ != height_) {
        width_ = pending_width_;
        height_ = pending_height_;
        if (width_ == 0 || height_ == 0) {
            return false;
        }

        gpu_->wait_idle();
        gpu_->resize_swapchain(swapchain_, width_, height_);
        transitioned_mask_ = 0;
    }

    if (width_ == 0 || height_ == 0) {
        return false;
    }

    if (frame_n_ >= frames_in_flight_) {
        gpu_->wait_semaphore(frame_sem_, frame_n_ + 1 - frames_in_flight_);
    }

    uint32_t slot = frame_index();
    uint64_t oldest = frame_n_ >= frames_in_flight_ ? frames_[slot].ring_end : 0;
    frame_ring_.reclaim(oldest);

    process_delay_deletes(false);

    Frame& fr = frames_[slot];
    gpu_->reset_cmd_pool(fr.pool);

    if (!gpu_->swapchain_acquire(swapchain_, current_slot_)) {
        gpu_->wait_idle();
        gpu_->resize_swapchain(swapchain_, width_, height_);
        transitioned_mask_ = 0;
        return false;
    }

    if ((transitioned_mask_ & (1u << current_slot_)) == 0) {
        GpuCmd cmd = gpu_->start_command_recording(fr.pool);
        gpu_->cmd_image_barrier(cmd, gpu_->swapchain_image(swapchain_, current_slot_), ResourceState::Undefined, ResourceState::PresentSrc);
        gpu_->submit(queue_, {&cmd, 1});
        transitioned_mask_ |= 1u << current_slot_;
    }

    in_frame_ = true;
    return true;
}

void RenderCore::end_frame() {
    upload_flush();
    if (frame_cmds_.empty()) {
        new_frame_cmd();
    }

    gpu_->cmd_signal_semaphore(frame_cmds_.back(), frame_sem_, frame_n_ + 1);
    gpu_->submit(queue_, frame_cmds_);
    frame_cmds_.clear();

    gpu_->swapchain_present(queue_, swapchain_, current_slot_, frame_sem_, frame_n_ + 1);
    frames_[frame_index()].ring_end = frame_ring_.total;
    ++frame_n_;
    in_frame_ = false;
}

GpuCmd RenderCore::new_frame_cmd() {
    GpuCmd cmd = gpu_->start_command_recording(frames_[frame_n_ % frames_in_flight_].pool);
    frame_cmds_.push_back(cmd);
    return cmd;
}

GpuCmd RenderCore::new_upload_cmd() {
    UploadSlot& s = upload_slots_[upload_active_];
    if (current_upload_cmd_ == k_gpu_invalid) {
        if (s.last_value != 0) {
            gpu_->wait_semaphore(upload_sem_, s.last_value);
        }

        gpu_->reset_cmd_pool(s.pool);
        current_upload_cmd_ = gpu_->start_command_recording(s.pool);
    }

    return current_upload_cmd_;
}

uint32_t RenderCore::frame_alloc(uint32_t size) {
    uint32_t off = frame_ring_.alloc(size);
    if (off == UINT32_MAX) {
        LOGE("frame_alloc: out of frame budget (%u bytes)", size);
    }

    return off;
}

uint32_t RenderCore::upload_alloc(uint32_t size) {
    for (;;) {
        upload_reclaim();
        uint32_t off = upload_ring_.alloc(size);
        if (off != UINT32_MAX) {
            return off;
        }

        uint64_t oldest = UINT64_MAX;
        for (auto& s : upload_slots_) {
            if (s.last_value != 0 && s.ring_end > upload_ring_.tail && s.last_value < oldest) {
                oldest = s.last_value;
            }
        }

        if (oldest != UINT64_MAX) {
            gpu_->wait_semaphore(upload_sem_, oldest);
        } else if (current_upload_cmd_ != k_gpu_invalid) {
            upload_flush();
        } else {
            LOGE("upload_alloc: ring exhausted (%u bytes)", size);
            return UINT32_MAX;
        }
    }
}

void RenderCore::delay_delete_image(GpuImage image) {
    delay_deletes_.push_back({.image = image, .frame = frame_n_});
}

void RenderCore::delay_delete_buffer(GpuBuffer buffer) {
    delay_deletes_.push_back({.buffer = buffer, .frame = frame_n_});
}

void RenderCore::delay_delete_pipeline(GpuPipeline pipeline) {
    delay_deletes_.push_back({.pipeline = pipeline, .frame = frame_n_});
}

void RenderCore::delay_delete_view(GpuImageView view) {
    delay_deletes_.push_back({.view = view, .frame = frame_n_});
}

uint32_t RenderCore::heap_add_view(GpuImageView view) {
    uint32_t slot = alloc_resource_slot();
    if (slot == UINT32_MAX) {
        return k_invalid_slot;
    }

    gpu_->write_descriptor_image(resource_set_, 0, slot, view);
    view_slots_[view] = slot;
    return slot;
}

void RenderCore::heap_remove_view(GpuImageView view) {
    auto it = view_slots_.find(view);
    if (it == view_slots_.end()) {
        return;
    }

    uint32_t slot = it->second;
    view_slots_.erase(it);
    delay_deletes_.push_back({.view = view, .slot = slot, .frame = frame_n_});
}

void RenderCore::upload_flush() {
    if (current_upload_cmd_ == k_gpu_invalid) {
        return;
    }

    gpu_->cmd_signal_semaphore(current_upload_cmd_, upload_sem_, upload_n_ + 1);
    gpu_->submit(queue_, {&current_upload_cmd_, 1});
    UploadSlot& s = upload_slots_[upload_active_];
    s.last_value = ++upload_n_;
    s.ring_end = upload_ring_.total;
    upload_active_ = (upload_active_ + 1) % k_upload_slots;
    current_upload_cmd_ = k_gpu_invalid;
}

void RenderCore::upload_reclaim() {
    uint64_t done = gpu_->semaphore_value(upload_sem_);
    for (auto& s : upload_slots_) {
        if (s.last_value != 0 && s.last_value <= done && s.ring_end > upload_ring_.tail) {
            upload_ring_.tail = s.ring_end;
        }
    }

    upload_ring_.reclaim(upload_ring_.tail);
}

void RenderCore::process_delay_deletes(bool all) {
    while (!delay_deletes_.empty() && (all || delay_deletes_.front().frame + frames_in_flight_ < frame_n_)) {
        DelayDelete& d = delay_deletes_.front();
        if (d.image != k_gpu_invalid) {
            gpu_->destroy_image(d.image);
        }

        if (d.buffer != k_gpu_invalid) {
            gpu_->destroy_buffer(d.buffer);
        }

        if (d.pipeline != k_gpu_invalid) {
            gpu_->destroy_pipeline(d.pipeline);
        }

        if (d.view != k_gpu_invalid) {
            gpu_->destroy_image_view(d.view);
        }

        if (d.slot != k_invalid_slot) {
            free_resource_slots_[free_resource_top_++] = d.slot;
        }

        delay_deletes_.pop_front();
    }
}

uint32_t RenderCore::alloc_resource_slot() {
    if (free_resource_top_ == 0) {
        LOGE("RenderCore: resource heap slots exhausted");
        return UINT32_MAX;
    }

    return free_resource_slots_[--free_resource_top_];
}
