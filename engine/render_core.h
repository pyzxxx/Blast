#pragma once

#include "gpu_driver.h"

#include <deque>
#include <unordered_map>
#include <vector>

constexpr uint32_t k_invalid_slot = 0xFFFFFFFF;

struct Services;

class RenderCore {
public:
    RenderCore() = default;
    ~RenderCore() = default;
    RenderCore(const RenderCore&) = delete;
    RenderCore& operator=(const RenderCore&) = delete;

    bool init(const Services& services);
    void shutdown();
    void resize(uint32_t width, uint32_t height);

    bool begin_frame();
    void end_frame();

    GpuCmd new_frame_cmd();
    GpuCmd new_upload_cmd();

    uint32_t frame_alloc(uint32_t size);
    uint8_t* frame_cpu_addr() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return frame_ring_.map;
    }

    uint64_t frame_gpu_addr() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return frame_ring_.addr;
    }

    GpuBuffer frame_buffer() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return frame_ring_.buffer;
    }

    uint32_t upload_alloc(uint32_t size);
    uint8_t* upload_cpu_addr() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return upload_ring_.map;
    }

    GpuBuffer upload_buffer() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return upload_ring_.buffer;
    }

    void delay_delete_image(GpuImage);
    void delay_delete_buffer(GpuBuffer);
    void delay_delete_pipeline(GpuPipeline);
    void delay_delete_view(GpuImageView);

    uint32_t heap_add_view(GpuImageView view);
    void heap_remove_view(GpuImageView view);
    GpuDescriptorSet resource_set() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return resource_set_;
    }

    GpuDescriptorSetLayout resource_layout() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return resource_layout_;
    }

    GpuSwapchain swapchain() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return swapchain_;
    }

    uint32_t swapchain_slot() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return current_slot_;
    }

    uint32_t width() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return width_;
    }

    uint32_t height() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return height_;
    }

    uint32_t frames_in_flight() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return frames_in_flight_;
    }

    uint32_t frame_index() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return static_cast<uint32_t>(frame_n_ % frames_in_flight_);
    }

    uint64_t frame_value() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return frame_n_ + 1;
    }

    uint64_t completed_frame() const {
        BLAST_GPU_ASSERT_OWNER(owner_thread_);
        return gpu_->semaphore_value(frame_sem_);
    }

private:
    struct Ring {
        GpuBuffer buffer = k_gpu_invalid;
        uint8_t* map = nullptr;
        uint64_t addr = 0;
        uint32_t cap = 0;
        uint32_t head = 0;
        uint64_t budget = 0;
        uint64_t total = 0;
        uint64_t tail = 0;

        bool init(GpuDriver& gpu, uint32_t ring_cap, GpuBufferUsage usage);
        void shutdown(GpuDriver& gpu);
        uint32_t alloc(uint32_t size);
        void reclaim(uint64_t point);
    };

    struct Frame {
        GpuCmdPool pool = k_gpu_invalid;
        uint64_t ring_end = 0;
    };

    struct UploadSlot {
        GpuCmdPool pool = k_gpu_invalid;
        uint64_t last_value = 0;
        uint64_t ring_end = 0;
    };

    struct DelayDelete {
        GpuImage image = k_gpu_invalid;
        GpuBuffer buffer = k_gpu_invalid;
        GpuPipeline pipeline = k_gpu_invalid;
        GpuImageView view = k_gpu_invalid;
        uint32_t slot = k_invalid_slot;
        uint64_t frame = 0;
    };

    static constexpr uint32_t k_max_frames_in_flight = 3;
    static constexpr uint32_t k_frame_ring_size = 64 * 1024 * 1024;
    static constexpr uint32_t k_upload_ring_size = 64 * 1024 * 1024;
    static constexpr uint32_t k_ring_align = 256;
    static constexpr uint32_t k_upload_slots = 2;
    static constexpr uint32_t k_heap_descriptor_count = 65536;
    static constexpr uint32_t k_sampler_count = 64;

    void upload_flush();
    void upload_reclaim();
    void process_delay_deletes(bool all);
    uint32_t alloc_resource_slot();

    GpuDriver* gpu_ = nullptr;
    std::thread::id owner_thread_;
    GpuQueue queue_ = k_gpu_invalid;
    GpuSwapchain swapchain_ = k_gpu_invalid;
    GpuSemaphore frame_sem_ = k_gpu_invalid;
    GpuSemaphore upload_sem_ = k_gpu_invalid;

    Ring frame_ring_;
    Frame frames_[k_max_frames_in_flight];
    std::vector<GpuCmd> frame_cmds_;
    uint64_t frame_n_ = 0;
    uint32_t frames_in_flight_ = 1;

    Ring upload_ring_;
    UploadSlot upload_slots_[k_upload_slots];
    GpuCmd current_upload_cmd_ = k_gpu_invalid;
    uint64_t upload_n_ = 0;
    uint32_t upload_active_ = 0;

    GpuDescriptorSetLayout resource_layout_ = k_gpu_invalid;
    GpuDescriptorSet resource_set_ = k_gpu_invalid;
    GpuSampler default_sampler_ = k_gpu_invalid;
    std::unordered_map<GpuImageView, uint32_t> view_slots_;
    uint32_t* free_resource_slots_ = nullptr;
    uint32_t free_resource_top_ = 0;

    std::deque<DelayDelete> delay_deletes_;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t pending_width_ = 0;
    uint32_t pending_height_ = 0;
    uint32_t current_slot_ = 0;
    uint32_t transitioned_mask_ = 0;
    bool in_frame_ = false;
};
