#pragma once

#include "gpu_driver.h"
#include "render_core.h"
#include "shader_interop.h"
#include "shader_registry.h"
#include "world.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <unordered_map>

class MeshLibrary;
class MaterialLibrary;
class UiSystem;
class Vfs;
struct Services;

struct Pipelines {
    GpuPipeline mesh_depth = k_gpu_invalid;
    GpuPipeline mesh_depth_cutout = k_gpu_invalid;
    GpuPipeline mesh_main = k_gpu_invalid;
    GpuPipeline mesh_forward = k_gpu_invalid;
    GpuPipeline composite = k_gpu_invalid;
    GpuPipeline ui = k_gpu_invalid;
};

struct DrawCmd {
    GpuPipeline pipe = k_gpu_invalid;
    GpuBuffer vb = k_gpu_invalid;
    GpuBuffer ib = k_gpu_invalid;
    uint32_t attr_off = 0;
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    uint32_t base_vertex = 0;
    uint32_t first_record = 0;
    uint32_t instance_count = 1;
    uint64_t sort_key = 0;
};

struct DrawList {
    DrawCmd* cmds = nullptr;
    DrawCmd* tmp_cmds = nullptr;
    DrawRecord* tmp_records = nullptr;
    uint32_t* order = nullptr;
    DrawRecord* records = nullptr;
    uint64_t records_addr = 0;
    uint32_t count = 0;
    uint32_t cap = 0;
};

class Renderer {
public:
    bool init(const Services& services);
    void shutdown();

    void render();

private:
    static constexpr uint32_t k_max_draws = 16384;
    static constexpr uint32_t k_max_swapchain_slots = 8;

    GpuShader get_shader(ShaderId id);

    void declare_target(GpuCmd cmd, GpuImage& image, GpuFormat format, GpuImageUsage usage, ResourceState steady,
                        uint32_t fixed_size = 0);
    void declare_view(GpuImage image, GpuImageView& view, uint32_t* slot = nullptr, const GpuImageViewDesc& desc = {});
    void declare_pass(GpuRenderPass& slot, const GpuRenderPassDesc& desc);
    void declare_pipeline(GpuPipeline& slot, const GpuPipelineDesc& desc);
    void declare_frame(GpuCmd cmd);

    void collect();
    void write_draw(DrawList& list, const DrawCmd& cmd, uint32_t transform_index, uint32_t params_row);
    void sort_lists();
    void compact_list(DrawList& list);
    void execute_draws(GpuCmd cmd, const DrawList& list, uint64_t pass_addr);
    void pass_depth(GpuCmd cmd);
    void pass_main(GpuCmd cmd);
    void pass_composite(GpuCmd cmd);
    void draw_ui(GpuCmd cmd);

    GpuDriver* gpu_ = nullptr;
    RenderCore* rcore_ = nullptr;
    MeshLibrary* meshes_ = nullptr;
    MaterialLibrary* materials_ = nullptr;
    World* world_ = nullptr;
    UiSystem* ui_ = nullptr;
    Vfs* vfs_ = nullptr;

    GpuImage scene_color_ = k_gpu_invalid;
    GpuImageView scene_color_view_ = k_gpu_invalid;
    uint32_t scene_color_slot_ = k_invalid_slot;
    GpuImage depth_target_ = k_gpu_invalid;
    GpuImageView depth_target_view_ = k_gpu_invalid;

    GpuRenderPass depth_pass_ = k_gpu_invalid;
    GpuRenderPass main_pass_ = k_gpu_invalid;
    GpuRenderPass composite_passes_[k_max_swapchain_slots];
    uint32_t composite_pass_count_ = 0;
    Pipelines pipes_;
    GpuShader shaders_[static_cast<uint32_t>(ShaderId::Count)];
    std::unordered_map<GpuPipeline, GpuPipelineDesc> pipeline_descs_;
    std::unordered_map<GpuRenderPass, GpuRenderPassDesc> pass_descs_;

    bool target_recreated_ = false;

    GpuDescriptorSetLayout resource_layout_ = k_gpu_invalid;
    GpuDescriptorSet resource_set_ = k_gpu_invalid;
    GpuImage swap_current_ = k_gpu_invalid;

    DrawList draws_depth_;
    DrawList draws_main_;
    DrawList draws_forward_;

    MaterialParams default_params_ = {};
    glm::mat4* transforms_ = nullptr;
    uint64_t transforms_addr_ = 0;
    uint32_t transform_count_ = 0;
    uint8_t* frame_consts_cpu_ = nullptr;
    uint64_t frame_addr_ = 0;
    uint8_t* light_buf_cpu_ = nullptr;
    uint64_t light_buf_addr_ = 0;
    uint8_t* composite_cpu_ = nullptr;
    uint64_t composite_addr_ = 0;
};
