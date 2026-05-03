#ifndef RAY_TRACING_INTEROP_H
#define RAY_TRACING_INTEROP_H

#include "ShaderInterop.h"

struct GpuBVHNode
{
    sh_vec4 aabbMin;
    sh_vec4 aabbMax;
    sh_uint leftChild;
    sh_uint primitiveStart;
    sh_uint primitiveCount;
    sh_uint pad;
};

struct TLASInstance
{
    sh_mat4 worldToLocal;
    sh_mat4 localToWorld;
    sh_uvec2 bvhNodeAddress;
    sh_uvec2 triAddress;
    sh_uint bvhNodeCount;
    sh_uint triCount;
    sh_uint materialIndex;
    sh_uint pad;
};

struct TLASNode
{
    sh_vec4 aabbMin;
    sh_vec4 aabbMax;
    sh_uint leftChild;
    sh_uint instanceIndex;
    sh_uint pad[2];
};

#ifdef __cplusplus
static_assert(sizeof(GpuBVHNode) == 48, "GpuBVHNode size mismatch");
static_assert(sizeof(TLASInstance) == 160, "TLASInstance size mismatch");
static_assert(sizeof(TLASNode) == 48, "TLASNode size mismatch");
#endif

#endif
