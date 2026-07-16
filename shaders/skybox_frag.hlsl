#include "_GlobalBindings.hlsli"


struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    return skyboxCubemap.Sample(textureSamplers[SAMPLER_CLAMP_EDGE], input.uv);
}