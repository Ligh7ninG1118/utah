#include "_Clustering.hlsli"

groupshared uint gsKeys[256];


[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    uint2 dims;
    depthTarget.GetDimensions(dims.x, dims.y);

    // Compute cluster key (defaults to invalid key for sky/oob
    uint key = INVALID_CLUSTER_KEY;
    if (dtid.x < dims.x && dtid.y < dims.y)
    {
        float raw = depthTarget.Load(int3(dtid.xy, 0)).r;
        key = ClusterKeyFromDepth(raw, dtid.xy, dims);
    }

    gsKeys[gi] = key;
    GroupMemoryBarrierWithGroupSync();
    
    // bitonic sort
    for (uint k = 2; k <= 256; k <<= 1)
    {
        for (uint j = k >> 1; j > 0; j >>= 1)
        {
            uint partner = gi ^ j;
            if (partner > gi)
            {
                bool ascending = ((gi & k) == 0);
                uint a = gsKeys[gi], b = gsKeys[partner];
                if ((a > b) == ascending)
                {
                    gsKeys[gi] = b;
                    gsKeys[partner] = a;
                }
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }

    // dedup
    uint currSlotKey = gsKeys[gi];
    if (currSlotKey != INVALID_CLUSTER_KEY && (gi == 0 || gsKeys[gi - 1] != currSlotKey))
        InterlockedOr(clusterFlags[currSlotKey], 1u);
}