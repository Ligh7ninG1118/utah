#include "_GlobalBindings.hlsli"

groupshared uint gsKeys[256];


[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    uint2 dims;
    depthTarget.GetDimensions(dims.x, dims.y);
    
    // Compute cluster key
    uint key = INVALID_CLUSTER_KEY;
    if (dtid.x < dims.x && dtid.y < dims.y)
    {
        float raw = depthTarget.Load(int3(dtid.xy, 0)).r;
        if (raw > 0.0f)   // reverse z, 0.0f == sky
        {
            // reconstruct linear z
            float projA = cam.nearPlane / (cam.farPlane - cam.nearPlane);
            float projB = (cam.farPlane * cam.nearPlane) / (cam.farPlane - cam.nearPlane);
            float viewZ = projB / (raw + projA);
            float t = max(viewZ / cam.nearPlane, 1.0f); // log() guard
            // expoonential slicing
            uint zSlice = min(uint(floor(log(t) * (float(CLUSTER_GRID_Z) / log(cam.farPlane / cam.nearPlane)))),
                              CLUSTER_GRID_Z - 1);
           
            uint2 tileSize = (dims + uint2(CLUSTER_GRID_X, CLUSTER_GRID_Y) - 1)
                             / uint2(CLUSTER_GRID_X, CLUSTER_GRID_Y);
            uint2 xy = dtid.xy / tileSize;
            key = (zSlice * CLUSTER_GRID_Y + xy.y) * CLUSTER_GRID_X + xy.x;
        }
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