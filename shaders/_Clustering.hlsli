#ifndef UTAH_CLUSTERING_HLSLI
#define UTAH_CLUSTERING_HLSLI
#pragma once

#include "_GlobalBindings.hlsli"


// raw depth to linear depth
float ClusterLinearizeDepth(float raw)
{
    float projA = cam.nearPlane / (cam.farPlane - cam.nearPlane);
    float projB = (cam.farPlane * cam.nearPlane) / (cam.farPlane - cam.nearPlane);
    return projB / (raw + projA);
}

// exponential z slice index from view space z
uint CalculateClusterZSlice(float viewZ)
{
    float t = max(viewZ / cam.nearPlane, 1.0f);
    return min(uint(floor(log(t) * (float(CLUSTER_GRID_Z) / log(cam.farPlane / cam.nearPlane)))),
               CLUSTER_GRID_Z - 1);
}

uint2 ClusterTileSize(uint2 dims)
{
    return (dims + uint2(CLUSTER_GRID_X, CLUSTER_GRID_Y) - 1) / uint2(CLUSTER_GRID_X, CLUSTER_GRID_Y);
}

// key from pixel pos + depth
uint ClusterKeyFromDepth(float raw, uint2 pixel, uint2 dims)
{
    if (raw <= 0.0f) // reverse z, 0 == sky/far
        return INVALID_CLUSTER_KEY;
    
    uint zSlice = CalculateClusterZSlice(ClusterLinearizeDepth(raw));
    uint2 xy = pixel / ClusterTileSize(dims);
    return (zSlice * CLUSTER_GRID_Y + xy.y) * CLUSTER_GRID_X + xy.x;
}

void DecodeCluster(uint key, out uint i, out uint j, out uint k)
{
    k = key / (CLUSTER_GRID_X * CLUSTER_GRID_Y);
    uint rem = key % (CLUSTER_GRID_X * CLUSTER_GRID_Y);
    j = rem / CLUSTER_GRID_X;
    i = rem % CLUSTER_GRID_X;
}

// exponential slice boundary at index
float ClusterSliceDepth(uint s)
{
    return cam.nearPlane * pow(cam.farPlane / cam.nearPlane, float(s) / float(CLUSTER_GRID_Z));
}

// aabb of cluster, in view space
void ClusterViewAABB(uint i, uint j, uint k, uint2 dims, out float3 aabbMin, out float3 aabbMax)
{
    uint2 tileSize = ClusterTileSize(dims);
    uint2 minPixel = uint2(i, j) * tileSize;
    uint2 maxPixel = min((uint2(i, j) + 1) * tileSize, dims);

    float2 ndcMin = float2(minPixel) / float2(dims) * 2.0f - 1.0f;
    float2 ndcMax = float2(maxPixel) / float2(dims) * 2.0f - 1.0f;

    float zA = ClusterSliceDepth(k);
    float zB = ClusterSliceDepth(k + 1);

    aabbMin = float3(1e30, 1e30, 1e30);
    aabbMax = float3(-1e30, -1e30, -1e30);

    [unroll]
    for (uint c = 0; c < 8; c++)
    {
        float z = ((c & 1u) != 0u) ? zB : zA;
        float nx = ((c & 2u) != 0u) ? ndcMax.x : ndcMin.x;
        float ny = ((c & 4u) != 0u) ? ndcMax.y : ndcMin.y;
        float3 corner = float3(nx / cam.proj[0][0] * z, ny / cam.proj[1][1] * z, -z);
        aabbMin = min(aabbMin, corner);
        aabbMax = max(aabbMax, corner);
    }
}

bool IsAABBOverlapping(float3 aMin, float3 aMax, float3 bMin, float3 bMax)
{
    return all(aMin <= bMax) && all(aMax >= bMin);
}

bool IsSphereHittingAABB(float3 center, float radius, float3 bMin, float3 bMax)
{
    float3 q = clamp(center, bMin, bMax);
    float3 d = center - q;
    return dot(d, d) <= radius * radius;
}

#endif
