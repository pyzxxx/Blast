#ifndef COMMON_GLSL
#define COMMON_GLSL

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../../engine/shader_interop.h"

layout(push_constant) uniform PushBlock {
    PushData data;
} push;

layout(set = 0, binding = 0) uniform texture2D tex_heap[];
layout(set = 0, binding = 1) uniform sampler smp_heap[];

#define k_default_sampler 0u
#define k_invalid_slot 0xFFFFFFFFu

vec4 sample_tex(uint slot, vec2 uv) {
    return texture(sampler2D(tex_heap[nonuniformEXT(slot)], smp_heap[k_default_sampler]), uv);
}

layout(buffer_reference, std430) readonly buffer FrameConstsRef { FrameConsts value; };
layout(buffer_reference, std430) readonly buffer DrawRecordBuf { DrawRecord rows[]; };
layout(buffer_reference, std430) readonly buffer MaterialBuf { MaterialBlock rows[]; };
layout(buffer_reference, std430) readonly buffer TransformBuf { mat4 rows[]; };

FrameConsts frame_consts() {
    return FrameConstsRef(push.data.frame_addr).value;
}
DrawRecord draw_record(uint index) {
    return DrawRecordBuf(push.data.draw_addr).rows[index];
}
MaterialBlock material_row(uint row) {
    return MaterialBuf(frame_consts().params_buf_addr).rows[row];
}
mat4 transform_at(uint index) {
    return TransformBuf(frame_consts().transform_buf_addr).rows[index];
}

#endif
