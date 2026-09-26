#version 460
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"
#include "material.glsl"

#ifdef VARIANT_CUTOUT
layout(location = 0) in vec2 v_uv;
layout(location = 4) flat in uint v_draw_idx;
#endif

void main() {
#ifdef VARIANT_CUTOUT
    MaterialBlock mb = material_row(draw_record(v_draw_idx).params_row);
    float alpha = mb.params.base_color.a;
    if (mb.slot[k_tex_albedo] != k_invalid_slot) {
        alpha *= sample_tex(mb.slot[k_tex_albedo], v_uv).a;
    }
    if (!test_coverage(mb, alpha, uvec2(gl_FragCoord.xy))) {
        discard;
    }
#endif
}
