#ifndef SHADER_INTEROP_H
#define SHADER_INTEROP_H
#ifdef __cplusplus
#include <cstdint>
using uint = uint32_t;
#define SH_VEC4(name) float name[4]
#define SH_VEC2(name) float name[2]
#define SH_MAT4(name) float name[16]
#else
#define SH_VEC4(name) vec4 name
#define SH_VEC2(name) vec2 name
#define SH_MAT4(name) mat4 name
#endif

#define k_layer_opaque       0u
#define k_layer_transparent  1u
#define k_cutout_none        0u
#define k_cutout_mask        1u
#define k_cutout_dither      2u
#define k_mat_no_shadow      0x01u
#define k_mat_double_sided   0x04u
#define k_shade_unlit        0x01u

#define k_tex_albedo   0u
#define k_tex_normal   1u
#define k_tex_mr       2u
#define k_tex_count    8u

#define k_material_stride 128u

#define k_light_directional 0x1u
#define k_light_spot        0x2u

struct Light {
    SH_VEC4(pos_range);
    SH_VEC4(color_flags);
    SH_VEC4(direction);
    SH_VEC4(cones);
};
#ifdef __cplusplus
static_assert(sizeof(Light) == 64);
#endif

struct MaterialParams {
    uint     layer;
    uint     cutout;
    uint     flags;
    float    cutoff;
    SH_VEC4(base_color);
    SH_VEC4(emissive);
    float    metallic;
    float    roughness;
    uint     shade_flags;
    uint     pad[9];
};
struct MaterialBlock {
    MaterialParams params;
    uint slot[k_tex_count];
};
#ifdef __cplusplus
static_assert(sizeof(MaterialParams) == 96);
static_assert(sizeof(MaterialBlock) == k_material_stride);
#endif

struct PushData { uint64_t frame_addr, pass_addr, draw_addr; uint draw_idx; };

struct UiConsts {
    SH_VEC4(scale_translate);
    uint tex_slot;
    uint pad[3];
};
#ifdef __cplusplus
static_assert(sizeof(UiConsts) == 32);
#endif

struct DrawRecord {
    uint transform_index;
    uint params_row;
    uint pad[2];
};
struct FrameConsts {
    SH_MAT4(view_proj);
    SH_MAT4(view);
    SH_MAT4(inv_proj);
    SH_VEC4(camera_pos_time);
    uint64_t params_buf_addr, transform_buf_addr, light_buf_addr, cluster_buf_addr;
    uint     light_count;
    uint     dir_light_count;
    uint     cluster_dim[3];
    uint     tile_px;
    float    cluster_scale;
    float    cluster_bias;
    SH_VEC4(ambient);
};
#ifdef __cplusplus
static_assert(sizeof(FrameConsts) == 288);
#endif

#define k_cluster_max_lights 32u
struct ClusterList {
    uint count;
    uint indices[k_cluster_max_lights];
};
#ifdef __cplusplus
static_assert(sizeof(ClusterList) == 132);
#endif

#endif
