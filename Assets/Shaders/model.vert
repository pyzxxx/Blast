#version 460
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"
#include "material.glsl"

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_tangent;
layout(location = 3) in vec2 a_uv;

layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec4 v_tangent;
layout(location = 3) out vec2 v_uv;
layout(location = 4) flat out uint v_draw_idx;

void main() {
    FrameConsts fc = frame_consts();
    uint draw_idx = push.data.draw_idx + uint(gl_InstanceIndex);
    DrawRecord dr = draw_record(draw_idx);

    mat4 model = transform_at(dr.transform_index);

    vec4 world = model * vec4(a_pos, 1.0);

    v_world_pos = world.xyz;
    v_normal = mat3(model) * a_normal;
    v_tangent = vec4(mat3(model) * a_tangent.xyz, a_tangent.w);
    v_uv = a_uv;
    v_draw_idx = draw_idx;
    gl_Position = fc.view_proj * world;
}
