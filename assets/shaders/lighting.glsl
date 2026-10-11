#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "material.glsl"

layout(buffer_reference, std430) readonly buffer LightBuf { Light lights[]; };
layout(buffer_reference, std430) readonly buffer ClusterBuf { ClusterList lists[]; };

const float k_pi = 3.14159265;

float d_ggx(float ndh, float alpha) {
    float a2 = alpha * alpha;
    float denom = ndh * ndh * (a2 - 1.0) + 1.0;
    return a2 / (k_pi * denom * denom);
}

float v_smith_ggx_correlated(float ndv, float ndl, float alpha) {
    float a2 = alpha * alpha;
    float gv = ndl * sqrt(ndv * ndv * (1.0 - a2) + a2);
    float gl = ndv * sqrt(ndl * ndl * (1.0 - a2) + a2);
    return 0.5 / max(gv + gl, 1e-6);
}

vec3 f_schlick(vec3 f0, float vdh) {
    float p = pow(clamp(1.0 - vdh, 0.0, 1.0), 5.0);
    return f0 + (vec3(1.0) - f0) * p;
}

float distance_attenuation(float dist2, float range) {
    float range2 = range * range;
    float w = clamp(1.0 - dist2 * dist2 / (range2 * range2), 0.0, 1.0);
    return w * w / max(dist2, 1e-4);
}

vec3 shade_light(Surface s, vec3 world_pos, vec3 view_dir, Light L) {
    uint flags = floatBitsToUint(L.color_flags.w);
    vec3 ldir;
    float atten;
    if ((flags & k_light_directional) != 0u) {
        ldir = normalize(-L.pos_range.xyz);
        atten = 1.0;
    } else {
        vec3 d = L.pos_range.xyz - world_pos;
        float dist2 = dot(d, d);
        ldir = d * inversesqrt(max(dist2, 1e-12));
        atten = distance_attenuation(dist2, max(L.pos_range.w, 1e-6));
        if ((flags & k_light_spot) != 0u) {
            float cos_theta = dot(-ldir, L.direction.xyz);
            float cone = clamp((cos_theta - L.cones.y) / max(L.cones.x - L.cones.y, 1e-6), 0.0, 1.0);
            atten *= cone * cone;
        }
    }
    float ndl = max(dot(s.normal, ldir), 0.0);
    if (ndl <= 0.0 || atten <= 0.0) {
        return vec3(0.0);
    }

    vec3 h = normalize(view_dir + ldir);
    float ndv = max(dot(s.normal, view_dir), 1e-5);
    float ndh = max(dot(s.normal, h), 0.0);
    float vdh = max(dot(view_dir, h), 0.0);

    float alpha = s.roughness * s.roughness;
    vec3 f0 = mix(vec3(0.04), s.albedo, s.metallic);
    vec3 f = f_schlick(f0, vdh);

    vec3 fr = d_ggx(ndh, alpha) * v_smith_ggx_correlated(ndv, ndl, alpha) * f;
    vec3 fd = s.albedo * (vec3(1.0) - f) * (1.0 - s.metallic) / k_pi;

    return (fd + fr) * (ndl * atten) * L.color_flags.rgb;
}

vec3 light_all(Surface s, vec3 world_pos, vec3 view_dir, FrameConsts fc) {
    vec3 sum = s.albedo * fc.ambient.rgb / k_pi;
    LightBuf lb = LightBuf(fc.light_buf_addr);
    for (uint i = 0u; i < fc.dir_light_count; ++i) {
        sum += shade_light(s, world_pos, view_dir, lb.lights[i]);
    }

    float depth_vs = -(fc.view * vec4(world_pos, 1.0)).z;
    if (fc.cluster_buf_addr != uint64_t(0) && depth_vs > 0.0) {
        uvec2 dim_xy = uvec2(fc.cluster_dim[0], fc.cluster_dim[1]);
        uvec2 tile = min(uvec2(gl_FragCoord.xy) / fc.tile_px, dim_xy - 1u);
        uint slice = uint(clamp(log(depth_vs) * fc.cluster_scale + fc.cluster_bias, 0.0, float(fc.cluster_dim[2] - 1u)));
        uint cluster_id = (slice * dim_xy.y + tile.y) * dim_xy.x + tile.x;
        ClusterList cl = ClusterBuf(fc.cluster_buf_addr).lists[cluster_id];
        uint n = min(cl.count, k_cluster_max_lights);
        for (uint i = 0u; i < n; ++i) {
            sum += shade_light(s, world_pos, view_dir, lb.lights[cl.indices[i]]);
        }
    }
    return sum;
}

#endif
