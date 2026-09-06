#include "_Clustering.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

struct HeatmapPC
{
    float maxRef; // max light count to measure/divide against
};
[[vk::push_constant]] HeatmapPC pc;

// 0% black -> 25% blue -> 50% green -> 75% yellow -> 100% red, lerped in between 
float3 MapToHeatColor(float t)
{
    t = saturate(t);
    if (t < 0.25f) return lerp(float3(0, 0, 0), float3(0, 0, 1), t / 0.25f);
    if (t < 0.50f) return lerp(float3(0, 0, 1), float3(0, 1, 0), (t - 0.25f) / 0.25f);
    if (t < 0.75f) return lerp(float3(0, 1, 0), float3(1, 1, 0), (t - 0.50f) / 0.25f);
    return lerp(float3(1, 1, 0), float3(1, 0, 0), (t - 0.75f) / 0.25f);
}

float4 main(PSInput input) : SV_TARGET
{
    int3 pixel = int3(input.position.xy, 0);
    float raw = depthTarget.Load(pixel).r;

    uint2 dims;
    depthTarget.GetDimensions(dims.x, dims.y);

    uint key = ClusterKeyFromDepth(raw, uint2(pixel.xy), dims);
    if (key == INVALID_CLUSTER_KEY) // sky/oob
        discard;

    uint count = clusterLightGrid[key].y;
    float3 col = MapToHeatColor(float(count) / max(pc.maxRef, 1.0f));
    return float4(col, 0.85f); // alpha-blended over the scene
}
