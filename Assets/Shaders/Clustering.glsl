#ifndef CLUSTERING_GLSL
#define CLUSTERING_GLSL

uvec3 GetClusterCoord(vec2 fragCoord, float viewZ, uvec3 clusterSize, vec2 zNearFar, uvec2 screenSize, uvec2 rtSize)
{
    float zNear = zNearFar.x;
    float zFar = zNearFar.y;

    float viewDepth = -viewZ;
    float logDepth = log(viewDepth / zNear);
    float logRatio = log(zFar / zNear);
    uint zCoord = uint(logDepth * float(clusterSize.z) / logRatio);
    zCoord = clamp(zCoord, 0u, clusterSize.z - 1u);

    uvec2 tileSize = (screenSize + clusterSize.xy - 1u) / clusterSize.xy;

    vec2 screenPos = fragCoord * vec2(screenSize) / vec2(rtSize);

    uvec3 coord;
    coord.x = uint(screenPos.x) / tileSize.x;
    coord.y = uint(screenPos.y) / tileSize.y;
    coord.x = clamp(coord.x, 0u, clusterSize.x - 1u);
    coord.y = clamp(coord.y, 0u, clusterSize.y - 1u);
    coord.z = zCoord;

    return coord;
}

uint GetClusterIndex(uvec3 coord, uvec3 clusterSize)
{
    return coord.x + coord.y * clusterSize.x + coord.z * clusterSize.x * clusterSize.y;
}

#endif
