#include "_Clustering.hlsli"


[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint clusterId = dtid.x;
    if (clusterId >= CLUSTER_COUNT || clusterFlags[clusterId] == 0)
        return;

    uint2 dims;
    depthTarget.GetDimensions(dims.x, dims.y);

    uint i, j, k;
    DecodeCluster(clusterId, i, j, k);

    float3 cMin, cMax;
    ClusterViewAABB(i, j, k, dims, cMin, cMax);

    uint hits[MAX_LIGHTS_PER_CLUSTER];
    uint hitCount = 0;
    
    uint stack[BVH_STACK_SIZE];
    uint sp = 0;
    stack[sp++] = 0;

    while (sp > 0)
    {
        BVHNodeGPU node = bvhNodes[stack[--sp]];
        if (!IsAABBOverlapping(cMin, cMax, node.minAABB, node.maxAABB))
            continue;

        uint cnt = node.count & (~BVH_LEAF_BIT);
        bool isLeaf = (node.count & BVH_LEAF_BIT) != 0;

        if (isLeaf)
        {
            for (uint l = 0; l < cnt; l++)
            {
                uint li = node.firstIndex + l;
                float4 sphere = bvhLightSphere[li];
                if (IsSphereHittingAABB(sphere.xyz, sphere.w, cMin, cMax) && hitCount < MAX_LIGHTS_PER_CLUSTER)
                    hits[hitCount++] = bvhLightIndex[li];
            }
        }
        else
        {
            for (uint c = 0; c < cnt; c++)
            {
                if (sp < BVH_STACK_SIZE)
                    stack[sp++] = node.firstIndex + c;
            }
        }
    }

    uint base = 0;
    if (hitCount > 0)
    {
        InterlockedAdd(clusterLightList[0].count, hitCount, base);
        if (base + hitCount <= MAX_CLUSTER_LIGHT_INDICES)
        {
            for (uint m = 0; m < hitCount; m++)
                clusterLightList[0].list[base + m] = hits[m];
        }
        else
        {
            hitCount = 0; // overflow: drop this cluster's block
        }
    }

    clusterLightGrid[clusterId] = uint2(base, hitCount);
}
