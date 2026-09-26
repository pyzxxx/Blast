#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "common.glsl"

struct Surface {
    vec3 albedo;
    vec3 normal;
    float metallic;
    float roughness;
    vec3 emissive;
    float alpha;
    uint shade_flags;
};

float hash_pix(uvec2 pix) {
    uint h = pix.x * 0x8DA6B343u ^ pix.y * 0xD8163841u;
    h = h * 0xCB1AB31Fu;
    h ^= h >> 16u;
    h *= 0x9E3779B9u;
    h ^= h >> 16u;
    return float(h & 0x00FFFFFFu) * (1.0 / 16777216.0);
}

bool test_coverage(MaterialBlock mb, float alpha, uvec2 pix) {
    uint mode = mb.params.cutout;
    if (mode == k_cutout_mask) {
        return alpha >= mb.params.cutoff;
    }
    if (mode == k_cutout_dither) {
        return alpha > hash_pix(pix);
    }
    return true;
}

Surface eval_material(MaterialBlock mb, vec2 uv, vec3 n, vec4 t) {
    MaterialParams p = mb.params;
    Surface s;

    vec4 texel = mb.slot[k_tex_albedo] != k_invalid_slot
        ? sample_tex(mb.slot[k_tex_albedo], uv)
        : vec4(1.0);
    s.albedo = texel.rgb * p.base_color.rgb;
    s.alpha = texel.a * p.base_color.a;

    vec3 tn = vec3(0.0, 0.0, 1.0);
    if (mb.slot[k_tex_normal] != k_invalid_slot) {
        tn = sample_tex(mb.slot[k_tex_normal], uv).rgb * 2.0 - 1.0;
    }
    vec3 tang = normalize(t.xyz);
    vec3 norm = normalize(n);
    vec3 bitan = cross(norm, tang) * t.w;
    s.normal = normalize(tang * tn.x + bitan * tn.y + norm * tn.z);

    vec2 mr = vec2(1.0);
    if (mb.slot[k_tex_mr] != k_invalid_slot) {
        mr = sample_tex(mb.slot[k_tex_mr], uv).gb;
    }
    s.roughness = clamp(mr.x * p.roughness, 0.04, 1.0);
    s.metallic = clamp(mr.y * p.metallic, 0.0, 1.0);

    s.emissive = p.emissive.rgb;
    s.shade_flags = p.shade_flags;
    return s;
}

#endif
