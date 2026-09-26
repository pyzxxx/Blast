#version 460
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"
#include "material.glsl"

layout(location = 0) in vec3 a_pos;
layout(location = 3) in vec2 a_uv;

layout(location = 0) out vec2 v_uv;
layout(location = 4) flat out uint v_draw_idx;

void main() {
    FrameConsts fc = frame_consts();
    uint draw_idx = push.data.draw_idx + uint(gl_InstanceIndex);
    DrawRecord dr = draw_record(draw_idx);

    mat4 model = transform_at(dr.transform_index);

    vec4 world = model * vec4(a_pos, 1.0);

    v_uv = a_uv;
    v_draw_idx = draw_idx;
    gl_Position = fc.view_proj * world;
}
