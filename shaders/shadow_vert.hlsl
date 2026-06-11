static const uint MAX_SHADOW_CASTER_LIGHTS = 64;

struct ObjectData
{
    float4x4 model;
};

struct ShadowMapUBO
{
    float4x4 lightViewProj[MAX_SHADOW_CASTER_LIGHTS];
};

struct PushConstants
{
    uint objIndex;
    uint lightIndex;
};

[[vk::binding(2,0)]] StructuredBuffer<ObjectData> objBuf;
[[vk::binding(6,0)]] ConstantBuffer<ShadowMapUBO> shadowMapUBO;
[[vk::push_constant]] PushConstants pc;

struct VSInput
{
    [[vk::location(0)]] float3 inPos : POSITION;
};

float4 main(VSInput input, uint ViewIndex : SV_ViewID) : SV_POSITION
{
    
    float4x4 model = objBuf[pc.objIndex].model;
    return mul(shadowMapUBO.lightViewProj[pc.lightIndex + ViewIndex], mul(model, float4(input.inPos, 1.0)));
}