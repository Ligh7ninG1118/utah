#include "_GlobalBindings.hlsli"


struct PushConstants 
{ 
    uint objIndex; 
    uint matIndex; 
};

[[vk::push_constant]] PushConstants pc;

struct VSInput 
{
    [[vk::location(0)]] float3 inPos : POSITION;
};

struct VSOutput 
{
    float4 position : SV_Position;
    [[vk::location(0)]] nointerpolation uint matIndex : MATINDEX;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    float4x4 model  = objBuf[pc.objIndex].model;
    float4 worldPos = mul(model, float4(input.inPos, 1.0));
    o.position = mul(cam.proj, mul(cam.view, worldPos));
    o.matIndex = pc.matIndex;
    return o;
}
