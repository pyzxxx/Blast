#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "material.glsl"

layout(buffer_reference, std430) readonly buffer LightBuf { Light lights[]; };
layout(buffer_reference, std430) readonly buffer ClusterBuf { ClusterList lists[]; };
layout(buffer_reference, std430) readonly buffer ShadowBuf { ShadowEntry rows[]; };

const float k_pi = 3.14159265;

float shadow_sample(uint slot, vec2 uv, float depth) {
    return texture(sampler2DShadow(tex_heap[nonuniformEXT(slot)], smp_heap[k_smp_shadow_compare]), vec3(uv, depth));
}

float shadow_map_depth(uint slot, vec2 uv) {
    return texture(sampler2D(tex_heap[nonuniformEXT(slot)], smp_heap[k_smp_linear_clamp_border]), uv).r;
}

const vec2 k_pcss_poisson[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790));

mat2 shadow_rot(uvec2 pix) {
    float a = hash_pix(pix) * 6.2831853;
    float c = cos(a);
    float s = sin(a);
    return mat2(c, s, -s, c);
}

float shadow_dir_sample(uint entry_idx, vec3 world_pos, vec3 normal, float light_size, FrameConsts fc) {
    ShadowEntry se = ShadowBuf(fc.shadow_buf_addr).rows[entry_idx];
    float ls = light_size > 0.0 ? light_size : se.params.w;
    vec3 pos = world_pos + normal * se.params.z;
    vec4 sc = se.view_proj * vec4(pos, 1.0);
    if (sc.z >= 1.0) {
        return 1.0;
    }

    vec2 uv = sc.xy * 0.5 + 0.5;
    float ref_depth = sc.z - se.params.y;
    mat2 rot = shadow_rot(uvec2(gl_FragCoord.xy));

    // PCSS blocker search
    float search_world = clamp(ls * 16.0, 2.0 * se.texel_world, 0.5);
    vec2 search_uv = vec2(search_world / se.texel_world * se.params.x);
    float blocker_sum = 0.0;
    float blocker_count = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec2 off = rot * k_pcss_poisson[i] * search_uv;
        float d = shadow_map_depth(se.slot, uv + off);
        if (d < ref_depth) {
            blocker_sum += d;
            blocker_count += 1.0;
        }
    }

    if (blocker_count < 0.5) {
        return 1.0;
    }

    float penumbra_uv = se.params.x;
    if (ls > 0.0) {
        float blocker_avg = blocker_sum / blocker_count;
        float penumbra_world = max(ref_depth - blocker_avg, 0.0) * se.depth_range * ls;
        penumbra_uv = clamp(penumbra_world / se.texel_world * se.params.x, se.params.x, 64.0 * se.params.x);
    }

    float sum = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 off = rot * k_pcss_poisson[i] * vec2(penumbra_uv);
        sum += shadow_sample(se.slot, uv + off, ref_depth);
    }
    return sum / 16.0;
}

float shadow_directional(uint entry_base, vec3 world_pos, vec3 normal, float depth_vs, float light_size,
                         FrameConsts fc) {
    uint cascade = fc.shadow_cascade_count - 1u;
    for (uint i = 0u; i < fc.shadow_cascade_count; ++i) {
        if (depth_vs <= fc.shadow_cascade_splits[i]) {
            cascade = i;
            break;
        }
    }

    float s = shadow_dir_sample(entry_base + cascade, world_pos, normal, light_size, fc);

    if (cascade + 1u < fc.shadow_cascade_count) {
        float split = fc.shadow_cascade_splits[cascade];
        float prev = cascade > 0u ? fc.shadow_cascade_splits[cascade - 1u] : 0.0;
        float band = 0.1 * (split - prev);
        if (depth_vs > split - band) {
            float next = shadow_dir_sample(entry_base + cascade + 1u, world_pos, normal, light_size, fc);
            s = mix(s, next, clamp((depth_vs - (split - band)) / band, 0.0, 1.0));
        }
    }
    return s;
}

float shadow_local(uint entry_base, bool is_spot, vec3 world_pos, vec3 normal, vec3 light_pos, FrameConsts fc) {
    uint idx = entry_base;
    if (!is_spot) {
        vec3 d = world_pos - light_pos;
        vec3 ad = abs(d);
        uint face = 4u;
        if (ad.x >= ad.y && ad.x >= ad.z) {
            face = d.x > 0.0 ? 0u : 1u;
        } else if (ad.y >= ad.z) {
            face = d.y > 0.0 ? 2u : 3u;
        } else {
            face = d.z > 0.0 ? 4u : 5u;
        }
        idx += face;
    }

    ShadowEntry se = ShadowBuf(fc.shadow_buf_addr).rows[idx];
    vec4 sc = se.view_proj * vec4(world_pos + normal * se.params.z, 1.0);
    if (sc.w <= 0.0) {
        return 1.0;
    }

    vec3 ndc = sc.xyz / sc.w;
    if (ndc.z >= 1.0 || ndc.z < 0.0) {
        return 1.0;
    }

    vec2 uv = ndc.xy * 0.5 + 0.5;
    float depth = ndc.z - se.params.y;
    mat2 rot = shadow_rot(uvec2(gl_FragCoord.xy));
    float sum = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec2 off = rot * k_pcss_poisson[i] * (1.5 * se.params.x);
        sum += shadow_sample(se.slot, uv + off, depth);
    }
    return sum / 8.0;
}

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

vec3 shade_light(Surface s, vec3 world_pos, vec3 view_dir, Light L, FrameConsts fc) {
    uint flags = floatBitsToUint(L.color_flags.w);
    vec3 ldir;
    float atten;
    float shadow = 1.0;
    if ((flags & k_light_directional) != 0u) {
        ldir = normalize(-L.pos_range.xyz);
        atten = 1.0;
        int shadow_base = int(L.cones.z);
        if (shadow_base >= 0 && fc.shadow_cascade_count > 0u && (s.flags & k_mat_no_shadow) == 0u) {
            float depth_vs = -(fc.view * vec4(world_pos, 1.0)).z;
            shadow = shadow_directional(uint(shadow_base), world_pos, s.normal, depth_vs, L.cones.w, fc);
        }
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

        int shadow_base = int(L.cones.z);
        if (shadow_base >= 0 && (s.flags & k_mat_no_shadow) == 0u) {
            shadow = shadow_local(uint(shadow_base), (flags & k_light_spot) != 0u, world_pos, s.normal,
                                  L.pos_range.xyz, fc);
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

    return (fd + fr) * (ndl * atten * shadow) * L.color_flags.rgb;
}

vec3 light_all(Surface s, vec3 world_pos, vec3 view_dir, FrameConsts fc) {
    vec3 sum = s.albedo * fc.ambient.rgb / k_pi;
    LightBuf lb = LightBuf(fc.light_buf_addr);
    for (uint i = 0u; i < fc.dir_light_count; ++i) {
        sum += shade_light(s, world_pos, view_dir, lb.lights[i], fc);
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
            sum += shade_light(s, world_pos, view_dir, lb.lights[cl.indices[i]], fc);
        }
    }
    return sum;
}

#endif
