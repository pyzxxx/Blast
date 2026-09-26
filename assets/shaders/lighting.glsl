#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "material.glsl"

#define k_light_directional 0x1u

struct Light {
    vec4 pos_range;
    vec4 color_flags;
};

layout(buffer_reference, std430) readonly buffer LightBuf { Light lights[]; };

vec3 shade_light(Surface s, vec3 world_pos, vec3 view_dir, Light L) {
    vec3 ldir;
    float atten;
    if ((floatBitsToUint(L.color_flags.w) & k_light_directional) != 0u) {
        ldir = normalize(-L.pos_range.xyz);
        atten = 1.0;
    } else {
        vec3 d = L.pos_range.xyz - world_pos;
        float dist = length(d);
        ldir = d / max(dist, 1e-6);
        atten = clamp(1.0 - dist / max(L.pos_range.w, 1e-6), 0.0, 1.0);
        atten *= atten;
    }
    float ndl = max(dot(s.normal, ldir), 0.0);
    if (ndl <= 0.0 || atten <= 0.0) {
        return vec3(0.0);
    }
    vec3 h = normalize(view_dir + ldir);
    float a = s.roughness * s.roughness;
    float a2 = a * a;
    float ndh = max(dot(s.normal, h), 0.0);
    float denom = ndh * ndh * (a2 - 1.0) + 1.0;
    float spec = a2 / (3.14159265 * denom * denom);
    vec3 f0 = mix(vec3(0.04), s.albedo, s.metallic);
    vec3 diffuse = s.albedo * (1.0 - s.metallic);
    return (diffuse + f0 * spec) * ndl * atten * L.color_flags.rgb;
}

vec3 light_all(Surface s, vec3 world_pos, vec3 view_dir, FrameConsts fc) {
    vec3 sum = s.albedo * 0.03;
    LightBuf lb = LightBuf(fc.light_buf_addr);
    for (uint i = 0u; i < fc.light_count; ++i) {
        sum += shade_light(s, world_pos, view_dir, lb.lights[i]);
    }
    return sum;
}

#endif
