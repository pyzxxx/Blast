#ifndef BVH_TRACE_GLSL
#define BVH_TRACE_GLSL

struct RayHit
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    float t;
    uint materialIndex;
    bool valid;
};

vec3 UnpackSignedVector3x10_1x2(uint packed)
{
    int ix = int(packed << 22) >> 22;
    int iy = int(packed << 12) >> 22;
    int iz = int(packed << 2) >> 22;
    return vec3(float(ix) / 511.0, float(iy) / 511.0, float(iz) / 511.0);
}

float IntersectAABB(vec3 rayOrigin, vec3 invRayDir, vec3 aabbMin, vec3 aabbMax)
{
    vec3 t0 = (aabbMin - rayOrigin) * invRayDir;
    vec3 t1 = (aabbMax - rayOrigin) * invRayDir;
    vec3 tMin = min(t0, t1);
    vec3 tMax = max(t0, t1);
    float tEnter = max(max(tMin.x, tMin.y), tMin.z);
    float tExit = min(min(tMax.x, tMax.y), tMax.z);
    if (tEnter > tExit || tExit < 0.0)
        return 1e30;
    return max(tEnter, 0.0);
}

bool IntersectTriangle(vec3 rayOrigin, vec3 rayDir, vec3 v0, vec3 v1, vec3 v2,
                       out float t, out vec3 outNormal, out vec2 outUV)
{
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec3 h = cross(rayDir, e2);
    float a = dot(e1, h);
    if (abs(a) < 1e-6)
        return false;

    float f = 1.0 / a;
    vec3 s = rayOrigin - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0)
        return false;

    vec3 q = cross(s, e1);
    float v = f * dot(rayDir, q);
    if (v < 0.0 || u + v > 1.0)
        return false;

    t = f * dot(e2, q);
    if (t < 1e-4)
        return false;

    outNormal = normalize(cross(e1, e2));
    outUV = vec2(u, v);
    return true;
}

RayHit TraceBLAS(vec3 rayOrigin, vec3 rayDir, uvec2 nodeAddress, uvec2 triAddress,
                 uint nodeCount, uint triCount)
{
    RayHit hit;
    hit.valid = false;
    hit.t = 1e30;

    if (nodeCount == 0u)
        return hit;

    uint stack[32];
    int stackPtr = 0;
    stack[stackPtr++] = 0u;

    vec3 invRayDir = 1.0 / rayDir;

    BVHNodeBuffer nodeBuf = BVHNodeBuffer(nodeAddress);
    TriangleBuffer triBuf = TriangleBuffer(triAddress);

    while (stackPtr > 0 && stackPtr < 32)
    {
        uint nodeIdx = stack[--stackPtr];
        GpuBVHNode node = nodeBuf.data[nodeIdx];

        float tAABB = IntersectAABB(rayOrigin, invRayDir, node.aabbMin.xyz, node.aabbMax.xyz);
        if (tAABB >= hit.t)
            continue;

        uint leftChild = node.leftChild;
        if (leftChild == 0xFFFFFFFFu)
        {
            uint primStart = node.primitiveStart;
            uint primCount = node.primitiveCount;
            for (uint i = 0u; i < primCount; ++i)
            {
                uint triIdx = primStart + i;
                GPUTriangle tri = triBuf.data[triIdx];
                float tTri;
                vec3 triNormal;
                vec2 triUV;
                if (IntersectTriangle(rayOrigin, rayDir, tri.v0.xyz, tri.v1.xyz, tri.v2.xyz, tTri, triNormal, triUV))
                {
                    if (tTri < hit.t)
                    {
                        float w = 1.0 - triUV.x - triUV.y;
                        vec3 n0 = UnpackSignedVector3x10_1x2(floatBitsToUint(tri.v0.w));
                        vec3 n1 = UnpackSignedVector3x10_1x2(floatBitsToUint(tri.v1.w));
                        vec3 n2 = UnpackSignedVector3x10_1x2(floatBitsToUint(tri.v2.w));
                        vec2 uv0 = unpackHalf2x16(floatBitsToUint(tri.d0.x));
                        vec2 uv1 = unpackHalf2x16(floatBitsToUint(tri.d0.y));
                        vec2 uv2 = unpackHalf2x16(floatBitsToUint(tri.d0.z));
                        hit.t = tTri;
                        hit.position = rayOrigin + rayDir * tTri;
                        hit.normal = normalize(n0 * w + n1 * triUV.x + n2 * triUV.y);
                        hit.uv = uv0 * w + uv1 * triUV.x + uv2 * triUV.y;
                        hit.materialIndex = floatBitsToUint(tri.d0.w);
                        hit.valid = true;
                    }
                }
            }
        }
        else
        {
            if (stackPtr + 1 < 32)
            {
                stack[stackPtr++] = leftChild + 1;
                stack[stackPtr++] = leftChild;
            }
        }
    }

    return hit;
}

RayHit TraceTLAS(vec3 rayOrigin, vec3 rayDir)
{
    RayHit hit;
    hit.valid = false;
    hit.t = 1e30;

    uint stack[64];
    int stackPtr = 0;
    stack[stackPtr++] = 0u;

    vec3 invRayDir = 1.0 / rayDir;

    while (stackPtr > 0 && stackPtr < 64)
    {
        uint nodeIdx = stack[--stackPtr];
        TLASNode node = g_tlasNodes[nodeIdx];

        float tAABB = IntersectAABB(rayOrigin, invRayDir, node.aabbMin.xyz, node.aabbMax.xyz);
        if (tAABB >= hit.t)
            continue;

        uint leftChild = node.leftChild;
        uint instanceIdx = node.instanceIndex;

        if (leftChild == 0xFFFFFFFFu)
        {
            TLASInstance inst = g_tlasInstances[instanceIdx];
            vec3 localOrigin = (inst.worldToLocal * vec4(rayOrigin, 1.0)).xyz;
            vec3 localDir = normalize((inst.worldToLocal * vec4(rayDir, 0.0)).xyz);

            RayHit localHit = TraceBLAS(localOrigin, localDir, inst.bvhNodeAddress, inst.triAddress,
                                        inst.bvhNodeCount, inst.triCount);
            if (localHit.valid && localHit.t < hit.t)
            {
                hit = localHit;
                hit.position = (inst.localToWorld * vec4(localHit.position, 1.0)).xyz;
                hit.normal = normalize((transpose(inst.worldToLocal) * vec4(localHit.normal, 0.0)).xyz);
            }
        }
        else
        {
            if (stackPtr + 1 < 64)
            {
                stack[stackPtr++] = leftChild + 1;
                stack[stackPtr++] = leftChild;
            }
        }
    }

    return hit;
}

#endif
