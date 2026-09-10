#include "_GlobalBindings.hlsli"


struct PushConstants
{
    uint objIndex;
    uint lightIndex;
    uint matIndex;
};

[[vk::push_constant]] PushConstants pc;

struct VSInput
{
    [[vk::location(0)]] float3 inPos : POSITION;
    [[vk::location(2)]] float2 inUV  : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
    [[vk::location(1)]] nointerpolation uint matIndex : MATINDEX;
};

VSOutput main(VSInput input, uint ViewIndex : SV_ViewID)
{
    float4x4 model = objBuf[pc.objIndex].model;

    VSOutput o;
    o.position = mul(shadowMap.lightViewProj[pc.lightIndex + ViewIndex], mul(model, float4(input.inPos, 1.0)));
    o.uv = input.inUV;
    o.matIndex = pc.matIndex;
    return o;
}
