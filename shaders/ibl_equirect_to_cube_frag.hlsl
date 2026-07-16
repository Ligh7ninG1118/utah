#include "_GlobalBindings.hlsli"


struct PushConstants
{
    // reuse the same PC layout
    uint equirectIndex;
    // obsolete, using static sampler index now
    uint samplerIndex;
};

[[vk::push_constant]] PushConstants pc;

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 localPos : LOCALPOS;
};

static const float2 invAtan = float2(0.1591f, 0.3183f);

float2 SampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(-v.y));
    uv *= invAtan;
    uv += 0.5f;
    return uv;
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = SampleSphericalMap(normalize(input.localPos));
    float3 color = textures[pc.equirectIndex].SampleLevel(textureSamplers[SAMPLER_REPEAT_U_CLAMP_V], uv, 0.0f).rgb;
    
    return float4(color, 1.0f);
}