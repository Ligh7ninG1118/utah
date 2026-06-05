struct ObjectData
{
    float4x4 model;
};

struct ShadowMapUBO
{
    float4x4 lightViewProj;
};

struct PushConstants
{
    uint objIndex;
    uint matIndex;
};

[[vk::binding(2,0)]] StructuredBuffer<ObjectData> objBuf;
[[vk::binding(5,0)]] ConstantBuffer<ShadowMapUBO> shadowMapUBO;
[[vk::push_constant]] PushConstants pc;

struct VSInput
{
    [[vk::location(0)]] float3 inPos : POSITION;
};

float4 main(VSInput input) : SV_POSITION
{
    float4x4 model = objBuf[pc.objIndex].model;
    return mul(shadowMapUBO.lightViewProj, mul(model, float4(input.inPos, 1.0)));
}