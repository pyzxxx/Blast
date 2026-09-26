#version 460
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"
#include "material.glsl"
#include "lighting.glsl"

layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_tangent;
layout(location = 3) in vec2 v_uv;
layout(location = 4) flat in uint v_draw_idx;

layout(location = 0) out vec4 out_color;

void main() {
    FrameConsts fc = frame_consts();
    MaterialBlock mb = material_row(draw_record(v_draw_idx).params_row);

    Surface s = eval_material(mb, v_uv, v_normal, v_tangent);
    if (!test_coverage(mb, s.alpha, uvec2(gl_FragCoord.xy))) {
        discard;
    }

    if ((s.shade_flags & k_shade_unlit) != 0u) {
        out_color = vec4(s.albedo + s.emissive, s.alpha);
        return;
    }

    vec3 view_dir = normalize(fc.camera_pos_time.xyz - v_world_pos);
    vec3 lit = light_all(s, v_world_pos, view_dir, fc);
    out_color = vec4(lit + s.emissive, s.alpha);
}
