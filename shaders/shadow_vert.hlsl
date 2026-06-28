#include "_GlobalBindings.hlsli"


struct PushConstants
{
    uint objIndex;
    uint lightIndex;
};

[[vk::push_constant]] PushConstants pc;

struct VSInput
{
    [[vk::location(0)]] float3 inPos : POSITION;
};

float4 main(VSInput input, uint ViewIndex : SV_ViewID) : SV_POSITION
{
    float4x4 model = objBuf[pc.objIndex].model;
    return mul(shadowMap.lightViewProj[pc.lightIndex + ViewIndex], mul(model, float4(input.inPos, 1.0)));
}