#pragma once

#include "gpu_driver.h"
#include "render_core.h"
#include "scene_diff.h"
#include "shader_interop.h"
#include "shader_registry.h"
#include "world.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

class MeshLibrary;
class MaterialLibrary;
class AssetManager;
class UiSystem;
class Vfs;
struct Services;

struct Pipelines {
    GpuPipeline mesh_depth = k_gpu_invalid;
    GpuPipeline mesh_depth_cutout = k_gpu_invalid;
    GpuPipeline mesh_depth_ds = k_gpu_invalid;
    GpuPipeline mesh_depth_cutout_ds = k_gpu_invalid;
    GpuPipeline mesh_main = k_gpu_invalid;
    GpuPipeline mesh_main_ds = k_gpu_invalid;
    GpuPipeline mesh_forward = k_gpu_invalid;
    GpuPipeline mesh_forward_ds = k_gpu_invalid;
    GpuPipeline mesh_shadow = k_gpu_invalid;
    GpuPipeline mesh_shadow_cutout = k_gpu_invalid;
    GpuPipeline mesh_shadow_ds = k_gpu_invalid;
    GpuPipeline mesh_shadow_cutout_ds = k_gpu_invalid;
    GpuPipeline cluster = k_gpu_invalid;
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

    const float* ambient() const { return ambient_; }
    void set_ambient(float r, float g, float b) {
        ambient_[0] = r;
        ambient_[1] = g;
        ambient_[2] = b;
    }

    bool shadow_enabled() const { return shadow_enabled_; }
    void set_shadow_enabled(bool v) { shadow_enabled_ = v; }
    bool local_shadows_enabled() const { return local_shadows_enabled_; }
    void set_local_shadows_enabled(bool v) { local_shadows_enabled_ = v; }
    float shadow_distance() const { return shadow_distance_; }
    void set_shadow_distance(float v) { shadow_distance_ = v; }
    float shadow_pcss_radius() const { return shadow_pcss_radius_; }
    void set_shadow_pcss_radius(float v) { shadow_pcss_radius_ = v; }
    float shadow_depth_bias() const { return shadow_depth_bias_; }
    void set_shadow_depth_bias(float v) { shadow_depth_bias_ = v; }
    float shadow_normal_bias() const { return shadow_normal_bias_; }
    void set_shadow_normal_bias(float v) { shadow_normal_bias_ = v; }
    void shadow_stats(uint32_t& full, uint32_t& overlay, uint32_t& cached) const {
        full = stat_shadow_full_;
        overlay = stat_shadow_overlay_;
        cached = stat_shadow_cached_;
    }

    const SceneDiff& scene_diff() const { return scene_diff_; }

private:
    static constexpr uint32_t k_max_draws = 16384;
    static constexpr uint32_t k_max_swapchain_slots = 8;
    static constexpr uint32_t k_max_cascades = 4;
    static constexpr uint32_t k_cascade_res[k_max_cascades] = {2048, 2048, 1024, 1024};
    static constexpr uint32_t k_max_local_shadow_lights = 4;
    static constexpr uint32_t k_max_local_faces = 6;
    static constexpr uint32_t k_max_shadow_entries = k_max_cascades + k_max_local_shadow_lights * k_max_local_faces;
    static constexpr GpuFormat k_shadow_format = GpuFormat::D16Unorm;
    static constexpr uint32_t k_local_shadow_idle_frames = 120;

    struct ShadowCaster {
        DrawCmd cmd;
        DrawRecord rec;
        glm::vec3 wbmin;
        glm::vec3 wbmax;
        bool is_static;
    };

    enum class ShadowCull : uint32_t { StaticOnly, DynamicOnly };

    struct ShadowLayerState {
        bool valid = false;
        bool had_dynamics = false;
    };

    struct LocalShadow {
        ObjectHandle owner = k_object_invalid;
        glm::mat4 light_mat = glm::mat4(0.0f);
        uint32_t faces = 0;
        uint32_t res = 0;
        uint32_t entry_base = 0;
        float range = 0.0f;
        float spot_outer = 0.0f;
        uint32_t light_type = 0;
        uint32_t last_seen = 0;
        bool visible = false;
        GpuImage img = k_gpu_invalid;
        GpuImageView views[k_max_local_faces] = {};
        uint32_t slots[k_max_local_faces] = {};
        GpuRenderPass load_passes[k_max_local_faces] = {};
        GpuImage static_img = k_gpu_invalid;
        GpuImageView static_views[k_max_local_faces] = {};
        GpuRenderPass static_passes[k_max_local_faces] = {};
        ResourceState img_layout = ResourceState::Undefined;
        ResourceState static_layout = ResourceState::Undefined;
        ShadowLayerState layers[k_max_local_faces];
    };

    struct CsmSignature {
        glm::mat4 cam_world;
        glm::vec3 light_dir;
        float fov_y;
        float aspect;
        float near_z;
        float far_z;
        float shadow_dist;
        float pad = 0.0f;
    };

    GpuShader get_shader(ShaderId id);

    void declare_target(GpuCmd cmd, GpuImage& image, GpuFormat format, GpuImageUsage usage, ResourceState steady,
                        uint32_t fixed_size = 0, uint32_t layers = 1);
    void declare_view(GpuImage image, GpuImageView& view, uint32_t* slot = nullptr, const GpuImageViewDesc& desc = {});
    void declare_pass(GpuRenderPass& slot, const GpuRenderPassDesc& desc);
    void declare_pipeline(GpuPipeline& slot, const GpuPipelineDesc& desc);
    void declare_frame(GpuCmd cmd);

    void collect();
    void compute_csm(const glm::mat4& cam_world, float fov_y, float aspect, float near_z, float far_z,
                     const glm::vec3& light_dir, uint32_t light_row);
    void compute_local_shadow(ObjectHandle handle, const LightComponent& lc, const glm::mat4& world_mat,
                              uint32_t light_row, const glm::vec3& cam_pos, const glm::vec4* cam_planes);
    void destroy_local_shadow(LocalShadow& ls);
    uint32_t build_shadow_layer(const glm::mat4& vp, ShadowCull mode, DrawList& out);
    bool shadow_layer_dirty(const glm::mat4& vp, const ShadowLayerState& st) const;
    void transition(GpuCmd cmd, GpuImage img, ResourceState& layout, ResourceState to);
    void write_draw(DrawList& list, const DrawCmd& cmd, uint32_t transform_index, uint32_t params_row);
    void sort_lists();
    void compact_list(DrawList& list);
    void execute_draws(GpuCmd cmd, const DrawList& list, uint64_t pass_addr);
    void pass_cluster(GpuCmd cmd);
    void pass_shadow(GpuCmd cmd);
    void pass_depth(GpuCmd cmd);
    void pass_main(GpuCmd cmd);
    void pass_composite(GpuCmd cmd);
    void draw_ui(GpuCmd cmd);

    GpuDriver* gpu_ = nullptr;
    RenderCore* rcore_ = nullptr;
    MeshLibrary* meshes_ = nullptr;
    MaterialLibrary* materials_ = nullptr;
    AssetManager* assets_ = nullptr;
    World* world_ = nullptr;
    UiSystem* ui_ = nullptr;
    Vfs* vfs_ = nullptr;

    GpuImage scene_color_ = k_gpu_invalid;
    GpuImageView scene_color_view_ = k_gpu_invalid;
    uint32_t scene_color_slot_ = k_invalid_slot;
    GpuImage depth_target_ = k_gpu_invalid;
    GpuImageView depth_target_view_ = k_gpu_invalid;
    GpuImage shadow_maps_[k_max_cascades] = {};
    GpuImageView shadow_layer_views_[k_max_cascades] = {};
    uint32_t shadow_slots_[k_max_cascades] = {};
    GpuRenderPass shadow_load_passes_[k_max_cascades] = {};
    GpuImage shadow_static_maps_[k_max_cascades] = {};
    GpuImageView shadow_static_views_[k_max_cascades] = {};
    GpuRenderPass shadow_static_passes_[k_max_cascades] = {};
    ResourceState csm_img_layout_[k_max_cascades] = {};
    ResourceState csm_static_layout_[k_max_cascades] = {};
    ShadowLayerState csm_layers_[k_max_cascades] = {};
    CsmSignature csm_sig_ = {};
    bool csm_sig_valid_ = false;
    uint32_t dynamic_caster_count_ = 0;
    uint32_t stat_shadow_full_ = 0;
    uint32_t stat_shadow_overlay_ = 0;
    uint32_t stat_shadow_cached_ = 0;
    LocalShadow local_shadows_[k_max_local_shadow_lights] = {};
    uint32_t frame_index_ = 0;

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
    DrawList draws_shadow_;

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
    GpuBuffer cluster_buf_ = k_gpu_invalid;
    uint64_t cluster_buf_addr_ = 0;
    uint32_t cluster_dim_[3] = {0, 0, 0};
    uint8_t* shadow_buf_cpu_ = nullptr;
    uint64_t shadow_buf_addr_ = 0;
    uint32_t shadow_entry_count_ = 0;
    uint32_t local_shadow_count_ = 0;
    float shadow_splits_[k_max_cascades] = {};
    std::vector<ShadowCaster> casters_;
    SceneDiff scene_diff_;
    bool multi_shadow_warned_ = false;

    float ambient_[3] = {1.0f, 1.0f, 1.0f};
    float exposure_ = 1.0f;
    bool shadow_enabled_ = true;
    bool local_shadows_enabled_ = true;
    float shadow_distance_ = 150.0f;
    float shadow_pcss_radius_ = 0.02f;
    float shadow_depth_bias_ = 0.05f;
    float shadow_normal_bias_ = 2.0f;
};
