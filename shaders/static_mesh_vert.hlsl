#include "_GlobalBindings.hlsli"


struct PushConstants
{
    uint objIndex;
    uint matIndex;
};

[[vk::push_constant]] PushConstants pc;

struct VSInput 
{
    [[vk::location(0)]] float3 inPos    : POSITION;
    [[vk::location(1)]] float3 inNormal : NORMAL;
    [[vk::location(2)]] float2 inUV     : TEXCOORD0;
    [[vk::location(3)]] float4 inTangent: TANGENT;
    
};

struct VSOutput 
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPos          : WORLDPOS;
    [[vk::location(1)]] float3 normal            : NORMAL;
    [[vk::location(2)]] float2 uv                : TEXCOORD0;
    [[vk::location(3)]] nointerpolation uint matIndex : MATINDEX;
    [[vk::location(4)]] float4 tangent           : TANGENT;
    
};

VSOutput main(VSInput input)
{
    float4x4 model = objBuf[pc.objIndex].model;
    float4 worldPos = mul(model, float4(input.inPos, 1.0));
    
    VSOutput o;
    o.position = mul(cam.proj, mul(cam.view, worldPos));
    o.worldPos = worldPos.xyz;
    o.normal   = normalize(mul((float3x3)model, input.inNormal));
    o.uv       = input.inUV;
    o.matIndex = pc.matIndex;
    o.tangent  = float4(normalize(mul((float3x3)model, input.inTangent.xyz)), input.inTangent.w);
    return o;
}
