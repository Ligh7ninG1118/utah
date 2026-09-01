#include "_GlobalBindings.hlsli"


[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint id = dtid.x;
    if (id >= CLUSTER_COUNT)
        return; // 14*256 = 3584 > 3456
    
    if (clusterFlags[id] != 0)
    {
        uint slot;
        InterlockedAdd(clusterList[0].uniqueCount, 1, slot);
        clusterList[0].list[slot] = id; // deduped, unsorted
    }
}