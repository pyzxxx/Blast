#include "renderer.h"

#include "asset.h"
#include "log.h"
#include "material_library.h"
#include "mem.h"
#include "mesh_library.h"
#include "pak_format.h"
#include "services.h"
#include "ui_system.h"
#include "vfs.h"

#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <vector>

namespace {

struct CompositeConsts {
    uint32_t src_slot;
    float exposure;
    uint32_t pad[2];
};

bool blend_equal(const GpuBlendState& a, const GpuBlendState& b) {
    if (a.blend_enable != b.blend_enable) {
        return false;
    }

    if (a.src_color != b.src_color || a.dst_color != b.dst_color || a.color_op != b.color_op) {
        return false;
    }

    if (a.src_alpha != b.src_alpha || a.dst_alpha != b.dst_alpha || a.alpha_op != b.alpha_op) {
        return false;
    }

    return a.color_write_mask == b.color_write_mask;
}

bool stencil_face_equal(const GpuStencilFace& a, const GpuStencilFace& b) {
    if (a.fail_op != b.fail_op || a.depth_fail_op != b.depth_fail_op || a.pass_op != b.pass_op) {
        return false;
    }

    return a.compare == b.compare;
}

bool stencil_equal(const GpuStencilState& a, const GpuStencilState& b) {
    if (a.stencil_test != b.stencil_test || a.read_mask != b.read_mask || a.write_mask != b.write_mask) {
        return false;
    }

    return stencil_face_equal(a.front, b.front) && stencil_face_equal(a.back, b.back);
}

bool multisample_equal(const GpuMultisampleState& a, const GpuMultisampleState& b) {
    if (a.samples != b.samples || a.sample_shading != b.sample_shading || a.min_sample_shading != b.min_sample_shading) {
        return false;
    }

    return a.alpha_to_coverage == b.alpha_to_coverage && a.alpha_to_one == b.alpha_to_one;
}

bool raster_equal(const GpuRasterState& a, const GpuRasterState& b) {
    if (a.cull != b.cull || a.front_face != b.front_face || a.polygon_mode != b.polygon_mode) {
        return false;
    }

    if (a.depth_clamp != b.depth_clamp || a.line_width != b.line_width) {
        return false;
    }

    if (a.depth_bias_enable != b.depth_bias_enable || a.depth_bias_constant != b.depth_bias_constant ||
        a.depth_bias_clamp != b.depth_bias_clamp || a.depth_bias_slope != b.depth_bias_slope) {
        return false;
    }

    return true;
}

bool depth_equal(const GpuDepthState& a, const GpuDepthState& b) {
    return a.depth_test == b.depth_test && a.depth_write == b.depth_write && a.depth_compare == b.depth_compare;
}

bool pipeline_desc_equal(const GpuPipelineDesc& a, const GpuPipelineDesc& b) {
    // shaders & pass
    if (a.vs != b.vs || a.fs != b.fs || a.cs != b.cs || a.pass != b.pass) {
        return false;
    }

    // vertex input
    if (a.set_layout_count != b.set_layout_count) {
        return false;
    }

    for (uint32_t i = 0; i < a.set_layout_count; ++i) {
        if (a.set_layouts[i] != b.set_layouts[i]) {
            return false;
        }
    }

    if (a.vertex_input.attr_count != b.vertex_input.attr_count) {
        return false;
    }

    for (uint32_t i = 0; i < a.vertex_input.attr_count; ++i) {
        const GpuVertexAttr& x = a.vertex_input.attrs[i];
        const GpuVertexAttr& y = b.vertex_input.attrs[i];
        if (x.location != y.location || x.stream != y.stream || x.offset != y.offset || x.format != y.format) {
            return false;
        }
    }

    if (a.vertex_input.stream_strides[0] != b.vertex_input.stream_strides[0] ||
        a.vertex_input.stream_strides[1] != b.vertex_input.stream_strides[1]) {
        return false;
    }

    if (a.topology != b.topology) {
        return false;
    }

    if (!raster_equal(a.raster, b.raster)) {
        return false;
    }

    if (!depth_equal(a.depth, b.depth)) {
        return false;
    }

    return blend_equal(a.blend, b.blend) && stencil_equal(a.stencil, b.stencil) &&
           multisample_equal(a.multisample, b.multisample);
}

bool render_pass_desc_equal(const GpuRenderPassDesc& a, const GpuRenderPassDesc& b) {
    if (a.color_count != b.color_count) {
        return false;
    }

    for (uint32_t i = 0; i < a.color_count; ++i) {
        if (a.colors[i].view != b.colors[i].view || a.colors[i].load != b.colors[i].load) {
            return false;
        }

        if (memcmp(a.colors[i].clear, b.colors[i].clear, sizeof(float) * 4) != 0) {
            return false;
        }
    }

    return a.depth.view == b.depth.view && a.depth.load == b.depth.load && a.depth.clear == b.depth.clear;
}

} // namespace

static uint32_t quantize_depth(float depth, float far_z) {
    float t = depth / far_z;
    if (t < 0.0f) {
        t = 0.0f;
    }

    if (t > 1.0f) {
        t = 1.0f;
    }

    return static_cast<uint32_t>(t * 1048575.0f);
}

static void extract_frustum(const glm::mat4& m, glm::vec4 planes[6]) {
    glm::vec4 r0 = {m[0][0], m[1][0], m[2][0], m[3][0]};
    glm::vec4 r1 = {m[0][1], m[1][1], m[2][1], m[3][1]};
    glm::vec4 r2 = {m[0][2], m[1][2], m[2][2], m[3][2]};
    glm::vec4 r3 = {m[0][3], m[1][3], m[2][3], m[3][3]};
    planes[0] = r3 + r0;
    planes[1] = r3 - r0;
    planes[2] = r3 + r1;
    planes[3] = r3 - r1;
    planes[4] = r2;
    planes[5] = r3 - r2;
}

static bool aabb_in_frustum(const glm::vec4 planes[6], const glm::vec3& bmin, const glm::vec3& bmax) {
    for (uint32_t i = 0; i < 6; ++i) {
        glm::vec3 p = {planes[i].x >= 0.0f ? bmax.x : bmin.x, planes[i].y >= 0.0f ? bmax.y : bmin.y,
                       planes[i].z >= 0.0f ? bmax.z : bmin.z};
        if (glm::dot(glm::vec3(planes[i]), p) + planes[i].w < 0.0f) {
            return false;
        }
    }

    return true;
}

bool Renderer::init(const Services& services) {
    gpu_ = services.gpu;
    rcore_ = services.rcore;
    vfs_ = services.vfs;
    world_ = services.world;
    ui_ = services.ui;
    meshes_ = &services.assets->meshes();
    materials_ = &services.assets->materials();
    resource_layout_ = rcore_->resource_layout();
    resource_set_ = rcore_->resource_set();
    for (GpuRenderPass& p : composite_passes_) {
        p = k_gpu_invalid;
    }

    DrawList* lists[3] = {&draws_depth_, &draws_main_, &draws_forward_};
    for (DrawList* list : lists) {
        list->cmds = static_cast<DrawCmd*>(memalloc(sizeof(DrawCmd) * k_max_draws));
        list->tmp_cmds = static_cast<DrawCmd*>(memalloc(sizeof(DrawCmd) * k_max_draws));
        list->tmp_records = static_cast<DrawRecord*>(memalloc(sizeof(DrawRecord) * k_max_draws));
        list->order = static_cast<uint32_t*>(memalloc(sizeof(uint32_t) * k_max_draws));
        if (!list->cmds || !list->tmp_cmds || !list->tmp_records || !list->order) {
            LOGE("Renderer: draw list alloc failed");
            return false;
        }

        list->cap = k_max_draws;
    }

    default_params_.layer = k_layer_opaque;
    default_params_.cutout = k_cutout_none;
    default_params_.base_color[0] = 1.0f;
    default_params_.base_color[1] = 1.0f;
    default_params_.base_color[2] = 1.0f;
    default_params_.base_color[3] = 1.0f;
    default_params_.roughness = 1.0f;

    for (GpuShader& s : shaders_) {
        s = k_gpu_invalid;
    }

    return true;
}

void Renderer::shutdown() {
    gpu_->wait_idle();
    DrawList* lists[3] = {&draws_depth_, &draws_main_, &draws_forward_};
    for (DrawList* list : lists) {
        memfree(list->cmds);
        memfree(list->tmp_cmds);
        memfree(list->tmp_records);
        memfree(list->order);
        list->cmds = nullptr;
        list->tmp_cmds = nullptr;
        list->tmp_records = nullptr;
        list->order = nullptr;
    }

    GpuPipeline* all_pipes[] = {&pipes_.mesh_depth, &pipes_.mesh_depth_cutout, &pipes_.mesh_main,
                                &pipes_.mesh_forward, &pipes_.composite, &pipes_.ui};
    for (GpuPipeline* p : all_pipes) {
        if (*p != k_gpu_invalid) {
            gpu_->destroy_pipeline(*p);
        }

        *p = k_gpu_invalid;
    }

    pipeline_descs_.clear();
    GpuRenderPass* passes[] = {&depth_pass_, &main_pass_};
    for (GpuRenderPass* p : passes) {
        if (*p != k_gpu_invalid) {
            gpu_->destroy_render_pass(*p);
        }

        *p = k_gpu_invalid;
    }

    for (uint32_t i = 0; i < composite_pass_count_; ++i) {
        gpu_->destroy_render_pass(composite_passes_[i]);
    }

    composite_pass_count_ = 0;
    pass_descs_.clear();
    for (GpuShader& s : shaders_) {
        if (s != k_gpu_invalid) {
            gpu_->destroy_shader(s);
        }

        s = k_gpu_invalid;
    }

    GpuImage* images[] = {&scene_color_, &depth_target_};
    for (GpuImage* img : images) {
        if (*img != k_gpu_invalid) {
            gpu_->destroy_image(*img);
        }

        *img = k_gpu_invalid;
    }

    if (scene_color_view_ != k_gpu_invalid) {
        rcore_->heap_remove_view(scene_color_view_);
        scene_color_view_ = k_gpu_invalid;
        scene_color_slot_ = k_invalid_slot;
    }

    GpuImageView* views[] = {&depth_target_view_};
    for (GpuImageView* v : views) {
        if (*v != k_gpu_invalid) {
            gpu_->destroy_image_view(*v);
        }

        *v = k_gpu_invalid;
    }
}

GpuShader Renderer::get_shader(ShaderId id) {
    if (id == ShaderId::Count) {
        return k_gpu_invalid;
    }

    GpuShader& s = shaders_[static_cast<uint32_t>(id)];
    if (s != k_gpu_invalid) {
        return s;
    }

    char path[256];
    if (!shader_spv_path(id, path, sizeof(path))) {
        LOGE("Renderer: bad shader path for id %u", static_cast<uint32_t>(id));
        return k_gpu_invalid;
    }

    File file = vfs_->open(path);
    if (!file.is_open()) {
        return k_gpu_invalid;
    }

    uint64_t size = file.size();
    if (size == 0 || (size & 3) != 0) {
        return k_gpu_invalid;
    }

    std::vector<uint32_t> spv(static_cast<size_t>(size / 4));
    if (file.read(spv.data(), size) != size) {
        LOGE("Renderer: spv read failed '%s'", path);
        return k_gpu_invalid;
    }

    s = gpu_->create_shader(spv);
    return s;
}

void Renderer::declare_target(GpuCmd cmd, GpuImage& image, GpuFormat format, GpuImageUsage usage,
                              ResourceState steady, uint32_t fixed_size) {
    target_recreated_ = false;

    uint32_t w = fixed_size != 0 ? fixed_size : rcore_->width();
    uint32_t h = fixed_size != 0 ? fixed_size : rcore_->height();
    if (image != k_gpu_invalid) {
        uint32_t bw = 0;
        uint32_t bh = 0;
        gpu_->image_size(image, bw, bh);
        if (bw == w && bh == h) {
            return;
        }

        rcore_->delay_delete_image(image);
        image = k_gpu_invalid;
    }

    GpuImageDesc d = {};
    d.width = w;
    d.height = h;
    d.format = format;
    d.usage = usage;
    image = gpu_->create_image(d);
    if (image == k_gpu_invalid) {
        LOGE("Renderer: target creation failed");
        return;
    }

    gpu_->cmd_image_barrier(cmd, image, ResourceState::Undefined, steady);
    target_recreated_ = true;
}

void Renderer::declare_view(GpuImage image, GpuImageView& view, uint32_t* slot, const GpuImageViewDesc& desc) {
    if (!target_recreated_) {
        return;
    }

    if (view != k_gpu_invalid) {
        if (slot != nullptr) {
            rcore_->heap_remove_view(view);
            *slot = k_invalid_slot;
        } else {
            rcore_->delay_delete_view(view);
        }
    }

    view = gpu_->create_image_view(image, desc);
    if (slot != nullptr) {
        *slot = rcore_->heap_add_view(view);
    }
}

void Renderer::declare_pass(GpuRenderPass& slot, const GpuRenderPassDesc& desc) {
    auto it = pass_descs_.find(slot);
    if (slot != k_gpu_invalid && (it == pass_descs_.end() || !render_pass_desc_equal(it->second, desc))) {
        gpu_->destroy_render_pass(slot);
        pass_descs_.erase(slot);
        slot = k_gpu_invalid;
    }

    if (slot == k_gpu_invalid) {
        slot = gpu_->create_render_pass(desc);
        if (slot != k_gpu_invalid) {
            pass_descs_[slot] = desc;
        } else {
            LOGE("Renderer: render pass creation failed");
        }
    }
}

void Renderer::declare_pipeline(GpuPipeline& slot, const GpuPipelineDesc& desc) {
    if (slot != k_gpu_invalid) {
        const GpuPipelineDesc& old = pipeline_descs_[slot];
        if (pipeline_desc_equal(old, desc)) {
            return;
        }

        rcore_->delay_delete_pipeline(slot);
        pipeline_descs_.erase(slot);
        slot = k_gpu_invalid;
    }

    if (desc.vs == k_gpu_invalid || desc.pass == k_gpu_invalid) {
        LOGE("Renderer: pipeline declaration incomplete");
        return;
    }

    slot = gpu_->create_pipeline(desc);
    if (slot != k_gpu_invalid) {
        pipeline_descs_[slot] = desc;
    }
}

void Renderer::declare_frame(GpuCmd cmd) {
    constexpr GpuImageUsage k_depth_usage = GpuImageUsage::DepthAttachment | GpuImageUsage::Sampled;
    constexpr GpuImageUsage k_scene_usage = GpuImageUsage::ColorAttachment | GpuImageUsage::Sampled;

    auto mesh_vertex_input = [](bool depth) {
        GpuVertexInput vi = {};
        vi.stream_strides[0] = sizeof(pak_format::VertexPos);
        vi.stream_strides[1] = sizeof(pak_format::VertexAttr);
        vi.attrs[0] = {.location = 0, .stream = 0, .offset = 0, .format = GpuFormat::R32G32B32Float};
        if (depth) {
            vi.attrs[1] = {.location = 3, .stream = 1, .offset = offsetof(pak_format::VertexAttr, uv), .format = GpuFormat::R32G32Float};
            vi.attr_count = 2;
        } else {
            vi.attrs[1] = {.location = 1, .stream = 1, .offset = offsetof(pak_format::VertexAttr, normal), .format = GpuFormat::R32G32B32Float};
            vi.attrs[2] = {.location = 2, .stream = 1, .offset = offsetof(pak_format::VertexAttr, tangent), .format = GpuFormat::R32G32B32A32Float};
            vi.attrs[3] = {.location = 3, .stream = 1, .offset = offsetof(pak_format::VertexAttr, uv), .format = GpuFormat::R32G32Float};
            vi.attr_count = 4;
        }

        return vi;
    };

    auto depth_pass_desc = [](GpuImageView depth) {
        GpuRenderPassDesc d = {};
        d.depth.view = depth;
        return d;
    };

    auto main_pass_desc = [](GpuImageView color, GpuImageView depth) {
        GpuRenderPassDesc d = {};
        d.colors[0].view = color;
        d.color_count = 1;
        d.depth.view = depth;
        d.depth.load = true;
        return d;
    };

    auto composite_pass_desc = [](GpuImageView color) {
        GpuRenderPassDesc d = {};
        d.colors[0].view = color;
        d.colors[0].clear[0] = 0.05f;
        d.colors[0].clear[1] = 0.05f;
        d.colors[0].clear[2] = 0.08f;
        d.colors[0].clear[3] = 1.0f;
        d.color_count = 1;
        return d;
    };

    auto base_pipe = [&]() {
        GpuPipelineDesc d = {};
        d.set_layouts[0] = resource_layout_;
        d.set_layout_count = 1;
        d.depth.depth_test = true;
        d.raster.cull = GpuCull::Back;
        // proj[1][1] *= -1 flips NDC Y, which inverts winding: CCW-authored fronts become CW
        d.raster.front_face = GpuFrontFace::Clockwise;
        return d;
    };

    auto mesh_depth_pipe = [&](GpuRenderPass pass, ShaderId vs, ShaderId fs = ShaderId::Count) {
        GpuPipelineDesc d = base_pipe();
        d.pass = pass;
        d.vs = get_shader(vs);
        d.fs = get_shader(fs);
        d.vertex_input = mesh_vertex_input(true);
        d.depth.depth_write = true;
        return d;
    };

    auto mesh_main_pipe = [&](GpuRenderPass pass, ShaderId vs, ShaderId fs, bool blend) {
        GpuPipelineDesc d = base_pipe();
        d.pass = pass;
        d.vs = get_shader(vs);
        d.fs = get_shader(fs);
        d.vertex_input = mesh_vertex_input(false);
        d.depth.depth_compare = GpuCompareOp::LessOrEqual;
        d.blend.blend_enable = blend;
        return d;
    };

    auto composite_pipe = [&](GpuRenderPass pass, ShaderId vs, ShaderId fs) {
        GpuPipelineDesc d = base_pipe();
        d.pass = pass;
        d.vs = get_shader(vs);
        d.fs = get_shader(fs);
        d.depth.depth_test = false;
        d.raster.cull = GpuCull::None;
        return d;
    };

    declare_target(cmd, depth_target_, GpuFormat::D32Float, k_depth_usage, ResourceState::DepthWrite);
    declare_view(depth_target_, depth_target_view_);

    declare_target(cmd, scene_color_, GpuFormat::R16G16B16A16Float, k_scene_usage, ResourceState::ColorAttachment);
    declare_view(scene_color_, scene_color_view_, &scene_color_slot_);

    declare_pass(depth_pass_, depth_pass_desc(depth_target_view_));
    declare_pass(main_pass_, main_pass_desc(scene_color_view_, depth_target_view_));

    declare_pipeline(pipes_.mesh_depth, mesh_depth_pipe(depth_pass_, ShaderId::DepthVert));
    declare_pipeline(pipes_.mesh_depth_cutout, mesh_depth_pipe(depth_pass_, ShaderId::DepthVert, ShaderId::DepthFrag_Cutout));
    declare_pipeline(pipes_.mesh_main, mesh_main_pipe(main_pass_, ShaderId::ModelVert, ShaderId::ModelFrag, false));
    declare_pipeline(pipes_.mesh_forward, mesh_main_pipe(main_pass_, ShaderId::ModelVert, ShaderId::ModelFrag, true));

    uint32_t slot_count = gpu_->swapchain_image_count(rcore_->swapchain());
    assert(slot_count <= k_max_swapchain_slots);

    for (uint32_t i = slot_count; i < composite_pass_count_; ++i) {
        gpu_->destroy_render_pass(composite_passes_[i]);
        pass_descs_.erase(composite_passes_[i]);
        composite_passes_[i] = k_gpu_invalid;
    }

    for (uint32_t i = 0; i < slot_count; ++i) {
        declare_pass(composite_passes_[i], composite_pass_desc(gpu_->swapchain_image_view(rcore_->swapchain(), i)));
    }

    composite_pass_count_ = slot_count;

    if (slot_count > 0) {
        declare_pipeline(pipes_.composite, composite_pipe(composite_passes_[0], ShaderId::CompositeVert, ShaderId::CompositeFrag));
    }

    auto ui_pipe = [&](GpuRenderPass pass) {
        GpuPipelineDesc d = {};
        d.pass = pass;
        d.vs = get_shader(ShaderId::UiVert);
        d.fs = get_shader(ShaderId::UiFrag);
        d.set_layouts[0] = resource_layout_;
        d.set_layout_count = 1;
        d.vertex_input.stream_strides[0] = sizeof(ImDrawVert);
        d.vertex_input.attrs[0] = {.location = 0, .stream = 0, .offset = 0, .format = GpuFormat::R32G32Float};
        d.vertex_input.attrs[1] = {.location = 1, .stream = 0, .offset = 8, .format = GpuFormat::R32G32Float};
        d.vertex_input.attrs[2] = {.location = 2, .stream = 0, .offset = 16, .format = GpuFormat::R8G8B8A8Unorm};
        d.vertex_input.attr_count = 3;
        d.raster.cull = GpuCull::None;
        d.blend.blend_enable = true;
        return d;
    };

    if (slot_count > 0) {
        declare_pipeline(pipes_.ui, ui_pipe(composite_passes_[0]));
    }
}

void Renderer::render() {
    GpuCmd cmd = rcore_->new_frame_cmd();
    declare_frame(cmd);
    collect();
    sort_lists();

    pass_depth(cmd);
    pass_main(cmd);
    pass_composite(cmd);
}

void Renderer::collect() {
    uint8_t* cpu = rcore_->frame_cpu_addr();
    uint64_t gpu = rcore_->frame_gpu_addr();

    auto alloc = [&](uint32_t size, uint8_t*& cpu_out, uint64_t& gpu_out) {
        uint32_t off = rcore_->frame_alloc(size);
        assert(off != UINT32_MAX);
        cpu_out = cpu + off;
        gpu_out = gpu + off;
    };

    DrawList* lists[3] = {&draws_depth_, &draws_main_, &draws_forward_};
    for (DrawList* list : lists) {
        list->count = 0;
        uint8_t* rec_cpu = nullptr;
        alloc(k_max_draws * static_cast<uint32_t>(sizeof(DrawRecord)), rec_cpu, list->records_addr);
        list->records = reinterpret_cast<DrawRecord*>(rec_cpu);
    }

    uint8_t* tr_cpu = nullptr;
    alloc(k_max_draws * static_cast<uint32_t>(sizeof(glm::mat4)), tr_cpu, transforms_addr_);
    transforms_ = reinterpret_cast<glm::mat4*>(tr_cpu);
    transform_count_ = 0;

    constexpr uint32_t k_max_lights = 8;
    constexpr uint32_t k_light_stride = 32;
    constexpr uint32_t k_light_flag_directional = 1u;

    alloc(static_cast<uint32_t>(sizeof(FrameConsts)), frame_consts_cpu_, frame_addr_);
    alloc(k_max_lights * k_light_stride, light_buf_cpu_, light_buf_addr_);
    memset(light_buf_cpu_, 0, k_max_lights * k_light_stride);
    alloc(static_cast<uint32_t>(sizeof(CompositeConsts)), composite_cpu_, composite_addr_);

    uint32_t light_total = 0;
    uint32_t light_count = 0;
    world_->each<LightComponent>([&](ObjectHandle e, LightComponent& lc) {
        ++light_total;
        if (light_count >= k_max_lights) {
            return;
        }

        Transform* t = world_->get<Transform>(e);
        glm::mat4 world_mat = t ? t->world_matrix() : glm::mat4(1.0f);
        const float* color = lc.color();
        uint8_t* slot = light_buf_cpu_ + light_count * k_light_stride;

        if (lc.light_type() == LightComponent::k_type_directional) {
            glm::vec3 dir = -glm::normalize(glm::vec3(world_mat[2]));
            uint32_t flags = k_light_flag_directional;
            memcpy(slot + 0, &dir, sizeof(glm::vec3));
            memcpy(slot + 16, color, sizeof(float) * 3);
            memcpy(slot + 28, &flags, sizeof(uint32_t));
        } else {
            glm::vec3 pos = glm::vec3(world_mat[3]);
            float range = lc.range();
            uint32_t flags = 0u;
            memcpy(slot + 0, &pos, sizeof(glm::vec3));
            memcpy(slot + 12, &range, sizeof(float));
            memcpy(slot + 16, color, sizeof(float) * 3);
            memcpy(slot + 28, &flags, sizeof(uint32_t));
        }

        ++light_count;
    });
    if (light_total > k_max_lights) {
        LOGW("Renderer: %u lights in world, only first %u used", light_total, k_max_lights);
    }

    ObjectHandle camera = k_object_invalid;
    world_->each<CameraComponent>([&](ObjectHandle h, CameraComponent&) {
        if (camera == k_object_invalid) {
            camera = h;
        }
    });

    Transform* cam_t = camera != k_object_invalid ? world_->get<Transform>(camera) : nullptr;
    CameraComponent* cam = camera != k_object_invalid ? world_->get<CameraComponent>(camera) : nullptr;
    glm::mat4 cam_world = cam_t ? cam_t->world_matrix() : glm::mat4(1.0f);
    float fov_y = cam ? cam->fov_y() : 1.0471976f;
    float near_z = cam ? cam->near_z() : 0.1f;
    float far_z = cam ? cam->far_z() : 1000.0f;
    float aspect = rcore_->height() > 0 ? static_cast<float>(rcore_->width()) / static_cast<float>(rcore_->height()) : 1.0f;

    glm::mat4 view = glm::inverse(cam_world);
    glm::mat4 proj = glm::perspective(fov_y, aspect, near_z, far_z);
    proj[1][1] *= -1.0f;
    glm::mat4 view_proj = proj * view;

    FrameConsts fc = {};
    memcpy(fc.view_proj, &view_proj, sizeof(fc.view_proj));
    memcpy(fc.view, &view, sizeof(fc.view));
    glm::vec3 cam_pos = glm::vec3(cam_world[3]);
    fc.camera_pos_time[0] = cam_pos.x;
    fc.camera_pos_time[1] = cam_pos.y;
    fc.camera_pos_time[2] = cam_pos.z;
    fc.camera_pos_time[3] = 0.0f;
    fc.light_count = light_count;
    fc.params_buf_addr = materials_->params_addr();
    fc.transform_buf_addr = transforms_addr_;
    fc.light_buf_addr = light_buf_addr_;
    memcpy(frame_consts_cpu_, &fc, sizeof(fc));

    glm::vec4 planes[6];
    extract_frustum(view_proj, planes);

    world_->each<MeshComponent>([&](ObjectHandle e, MeshComponent& mc) {
        MeshHandle mh = mc.mesh();
        if (mh == k_handle_invalid || !meshes_->valid(mh) || !meshes_->resident(mh)) {
            return;
        }

        const Mesh& mesh = meshes_->get(mh);
        if (!mesh.lods[0].resident) {
            return;
        }

        Transform* t = world_->get<Transform>(e);
        glm::mat4 world_mat = t ? t->world_matrix() : glm::mat4(1.0f);

        glm::vec3 lbmin = {mesh.bounds_min[0], mesh.bounds_min[1], mesh.bounds_min[2]};
        glm::vec3 lbmax = {mesh.bounds_max[0], mesh.bounds_max[1], mesh.bounds_max[2]};
        glm::vec3 wbmin = {FLT_MAX, FLT_MAX, FLT_MAX};
        glm::vec3 wbmax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (uint32_t c = 0; c < 8; ++c) {
            glm::vec3 corner = {(c & 1) ? lbmax.x : lbmin.x, (c & 2) ? lbmax.y : lbmin.y, (c & 4) ? lbmax.z : lbmin.z};
            glm::vec3 wp = glm::vec3(world_mat * glm::vec4(corner, 1.0f));
            wbmin = glm::min(wbmin, wp);
            wbmax = glm::max(wbmax, wp);
        }

        if (!aabb_in_frustum(planes, wbmin, wbmax)) {
            return;
        }

        MaterialHandle mat = mc.material();
        uint32_t row = 0;
        const MaterialParams* params = &default_params_;
        if (mat != k_handle_invalid && materials_->valid(mat) && materials_->resident(mat)) {
            row = materials_->index_of(mat);
            params = &materials_->get(mat).params;
        }

        assert(transform_count_ < k_max_draws);
        uint32_t transform_index = transform_count_++;
        transforms_[transform_index] = world_mat;

        glm::vec3 center = (wbmin + wbmax) * 0.5f;
        float view_z = glm::dot(glm::vec3(view[0][2], view[1][2], view[2][2]), center) + view[3][2];
        float depth = -view_z;
        uint32_t qdepth = quantize_depth(depth, far_z);
        uint64_t mesh_key = static_cast<uint64_t>(static_cast<uint32_t>(mh));

        DrawCmd base = {};
        base.vb = mesh.vb;
        base.ib = mesh.ib;
        base.attr_off = mesh.attr_off;
        base.first_index = mesh.lods[0].first_index;
        base.index_count = mesh.lods[0].index_count;
        base.base_vertex = mesh.lods[0].base_vertex;

        if (params->layer == k_layer_transparent) {
            base.pipe = pipes_.mesh_forward;
            base.sort_key = ~static_cast<uint64_t>(qdepth);
            write_draw(draws_forward_, base, transform_index, row);
            return;
        }

        base.pipe = params->cutout != k_cutout_none ? pipes_.mesh_depth_cutout : pipes_.mesh_depth;
        base.sort_key = ((base.pipe & 0xFF) << 56) | (mesh_key << 24) | qdepth;
        write_draw(draws_depth_, base, transform_index, row);

        base.pipe = pipes_.mesh_main;
        base.sort_key = ((base.pipe & 0xFF) << 56) | (mesh_key << 24) | qdepth;
        write_draw(draws_main_, base, transform_index, row);
    });
}

void Renderer::write_draw(DrawList& list, const DrawCmd& cmd, uint32_t transform_index, uint32_t params_row) {
    assert(list.count < list.cap);
    uint32_t i = list.count++;
    list.cmds[i] = cmd;
    list.order[i] = i;
    list.tmp_records[i] = {.transform_index = transform_index, .params_row = params_row, .pad = {0, 0}};
}

void Renderer::sort_lists() {
    DrawList* lists[3] = {&draws_depth_, &draws_main_, &draws_forward_};
    for (DrawList* list : lists) {
        std::sort(list->order, list->order + list->count,
                  [&](uint32_t a, uint32_t b) { return list->cmds[a].sort_key < list->cmds[b].sort_key; });
        compact_list(*list);
    }
}

void Renderer::compact_list(DrawList& list) {
    uint32_t out = 0;
    uint32_t rec_count = 0;
    uint32_t i = 0;
    while (i < list.count) {
        const DrawCmd& first = list.cmds[list.order[i]];
        DrawCmd merged = first;
        merged.first_record = rec_count;
        merged.instance_count = 0;
        uint32_t j = i;
        while (j < list.count) {
            const DrawCmd& c = list.cmds[list.order[j]];
            if (c.pipe != first.pipe || c.vb != first.vb || c.ib != first.ib || c.attr_off != first.attr_off ||
                c.first_index != first.first_index || c.index_count != first.index_count ||
                c.base_vertex != first.base_vertex) {
                break;
            }

            list.records[rec_count++] = list.tmp_records[list.order[j]];
            merged.instance_count++;
            ++j;
        }

        list.tmp_cmds[out++] = merged;
        i = j;
    }

    memcpy(list.cmds, list.tmp_cmds, out * sizeof(DrawCmd));
    for (uint32_t k = 0; k < out; ++k) {
        list.order[k] = k;
    }

    list.count = out;
}

void Renderer::execute_draws(GpuCmd cmd, const DrawList& list, uint64_t pass_addr) {
    GpuPipeline bound = k_gpu_invalid;
    for (uint32_t i = 0; i < list.count; ++i) {
        const DrawCmd& c = list.cmds[list.order[i]];
        if (c.pipe == k_gpu_invalid) {
            continue;
        }

        if (c.pipe != bound) {
            gpu_->cmd_bind_pipeline(cmd, c.pipe);
            gpu_->cmd_bind_descriptors(cmd, c.pipe, resource_set_);
            bound = c.pipe;
        }

        gpu_->cmd_bind_vertex_buffer(cmd, 0, c.vb, 0);
        gpu_->cmd_bind_vertex_buffer(cmd, 1, c.vb, c.attr_off);
        gpu_->cmd_bind_index_buffer(cmd, c.ib);
        PushData pd = {.frame_addr = frame_addr_,
                       .pass_addr = pass_addr,
                       .draw_addr = list.records_addr,
                       .draw_idx = c.first_record};
        gpu_->cmd_push_data(cmd, c.pipe, &pd, sizeof(pd));
        gpu_->cmd_draw_indexed(cmd, c.index_count, c.instance_count, c.first_index, static_cast<int32_t>(c.base_vertex), 0);
    }
}

void Renderer::pass_depth(GpuCmd cmd) {
    if (depth_pass_ == k_gpu_invalid) {
        return;
    }

    gpu_->cmd_begin_render_pass(cmd, depth_pass_);
    execute_draws(cmd, draws_depth_, 0);
    gpu_->cmd_end_render_pass(cmd);
    gpu_->cmd_image_barrier(cmd, depth_target_, ResourceState::DepthWrite, ResourceState::DepthWrite);
}

void Renderer::pass_main(GpuCmd cmd) {
    if (main_pass_ == k_gpu_invalid) {
        return;
    }

    gpu_->cmd_begin_render_pass(cmd, main_pass_);
    execute_draws(cmd, draws_main_, 0);
    execute_draws(cmd, draws_forward_, 0);
    gpu_->cmd_end_render_pass(cmd);
}

void Renderer::pass_composite(GpuCmd cmd) {
    swap_current_ = gpu_->swapchain_image(rcore_->swapchain(), rcore_->swapchain_slot());
    gpu_->cmd_image_barrier(cmd, swap_current_, ResourceState::PresentSrc, ResourceState::ColorAttachment);
    if (scene_color_ != k_gpu_invalid) {
        gpu_->cmd_image_barrier(cmd, scene_color_, ResourceState::ColorAttachment, ResourceState::ShaderRead);
    }

    uint32_t slot = rcore_->swapchain_slot();
    if (slot < composite_pass_count_ && pipes_.composite != k_gpu_invalid) {
        CompositeConsts cc = {};
        cc.src_slot = scene_color_slot_;
        cc.exposure = 1.0f;
        memcpy(composite_cpu_, &cc, sizeof(cc));
        gpu_->cmd_begin_render_pass(cmd, composite_passes_[slot]);
        gpu_->cmd_bind_pipeline(cmd, pipes_.composite);
        gpu_->cmd_bind_descriptors(cmd, pipes_.composite, resource_set_);
        PushData pd = {.frame_addr = frame_addr_, .pass_addr = composite_addr_, .draw_addr = 0, .draw_idx = 0};
        gpu_->cmd_push_data(cmd, pipes_.composite, &pd, sizeof(pd));
        gpu_->cmd_draw(cmd, 3, 0);
        draw_ui(cmd);
        gpu_->cmd_end_render_pass(cmd);
    }

    if (scene_color_ != k_gpu_invalid) {
        gpu_->cmd_image_barrier(cmd, scene_color_, ResourceState::ShaderRead, ResourceState::ColorAttachment);
    }

    gpu_->cmd_image_barrier(cmd, swap_current_, ResourceState::ColorAttachment, ResourceState::PresentSrc);
}

void Renderer::draw_ui(GpuCmd cmd) {
    const ImDrawData* dd = ui_->draw_data();
    uint32_t font_slot = ui_->font_slot();
    if (dd == nullptr || dd->TotalIdxCount == 0 || font_slot == k_invalid_slot || pipes_.ui == k_gpu_invalid) {
        return;
    }

    uint32_t cmd_count = 0;
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        cmd_count += static_cast<uint32_t>(dd->CmdLists[n]->CmdBuffer.Size);
    }

    uint8_t* cpu = rcore_->frame_cpu_addr();
    uint64_t gpu_base = rcore_->frame_gpu_addr();
    uint32_t vtx_off = rcore_->frame_alloc(dd->TotalVtxCount * static_cast<uint32_t>(sizeof(ImDrawVert)));
    uint32_t idx_off = rcore_->frame_alloc(dd->TotalIdxCount * static_cast<uint32_t>(sizeof(ImDrawIdx)));
    uint32_t consts_off = rcore_->frame_alloc(cmd_count * static_cast<uint32_t>(sizeof(UiConsts)));
    assert(vtx_off != UINT32_MAX && idx_off != UINT32_MAX && consts_off != UINT32_MAX);

    ImDrawVert* vtx = reinterpret_cast<ImDrawVert*>(cpu + vtx_off);
    ImDrawIdx* idx = reinterpret_cast<ImDrawIdx*>(cpu + idx_off);
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        const ImDrawList* list = dd->CmdLists[n];
        memcpy(vtx, list->VtxBuffer.Data, list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx, list->IdxBuffer.Data, list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx += list->VtxBuffer.Size;
        idx += list->IdxBuffer.Size;
    }

    float sx = 2.0f / dd->DisplaySize.x;
    float sy = 2.0f / dd->DisplaySize.y;
    float tx = -1.0f - dd->DisplayPos.x * sx;
    float ty = -1.0f - dd->DisplayPos.y * sy;
    float fb_w = static_cast<float>(rcore_->width());
    float fb_h = static_cast<float>(rcore_->height());

    gpu_->cmd_bind_pipeline(cmd, pipes_.ui);
    gpu_->cmd_bind_descriptors(cmd, pipes_.ui, resource_set_);
    gpu_->cmd_bind_vertex_buffer(cmd, 0, rcore_->frame_buffer(), vtx_off);
    gpu_->cmd_bind_index_buffer(cmd, rcore_->frame_buffer(), idx_off);

    UiConsts* consts = reinterpret_cast<UiConsts*>(cpu + consts_off);
    uint32_t ci = 0;
    uint32_t idx_base = 0;
    int32_t vtx_base = 0;
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        const ImDrawList* list = dd->CmdLists[n];
        for (int c = 0; c < list->CmdBuffer.Size; ++c) {
            const ImDrawCmd& dc = list->CmdBuffer[c];
            if (dc.ElemCount == 0) {
                continue;
            }

            float x0 = (dc.ClipRect.x - dd->DisplayPos.x) * dd->FramebufferScale.x;
            float y0 = (dc.ClipRect.y - dd->DisplayPos.y) * dd->FramebufferScale.y;
            float x1 = (dc.ClipRect.z - dd->DisplayPos.x) * dd->FramebufferScale.x;
            float y1 = (dc.ClipRect.w - dd->DisplayPos.y) * dd->FramebufferScale.y;
            x0 = x0 < 0.0f ? 0.0f : x0;
            y0 = y0 < 0.0f ? 0.0f : y0;
            x1 = x1 > fb_w ? fb_w : x1;
            y1 = y1 > fb_h ? fb_h : y1;
            if (x1 <= x0 || y1 <= y0) {
                continue;
            }

            UiConsts& uc = consts[ci++];
            uc.scale_translate[0] = sx;
            uc.scale_translate[1] = sy;
            uc.scale_translate[2] = tx;
            uc.scale_translate[3] = ty;
            uc.tex_slot = font_slot;
            gpu_->cmd_set_scissor(cmd, static_cast<int32_t>(x0), static_cast<int32_t>(y0),
                                  static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0));
            PushData pd = {.frame_addr = 0,
                           .pass_addr = gpu_base + consts_off + (ci - 1) * static_cast<uint32_t>(sizeof(UiConsts)),
                           .draw_addr = 0,
                           .draw_idx = 0};
            gpu_->cmd_push_data(cmd, pipes_.ui, &pd, sizeof(pd));
            gpu_->cmd_draw_indexed(cmd, dc.ElemCount, 1, idx_base + dc.IdxOffset,
                                   vtx_base + static_cast<int32_t>(dc.VtxOffset), 0);
        }

        idx_base += static_cast<uint32_t>(list->IdxBuffer.Size);
        vtx_base += list->VtxBuffer.Size;
    }
}
