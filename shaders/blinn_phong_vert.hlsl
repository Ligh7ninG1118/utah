struct CameraUBO
{
    float4x4 view;
    float4x4 proj;
};

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


[[vk::binding(0, 0)]] ConstantBuffer<CameraUBO> cam;

[[vk::binding(2, 0)]] StructuredBuffer<ObjectData> objBuf;

[[vk::binding(6, 0)]] ConstantBuffer<ShadowMapUBO> shadowMapUBO;

[[vk::push_constant]] PushConstants pc;

struct VSInput 
{
    [[vk::location(0)]] float3 inPos    : POSITION;
    [[vk::location(1)]] float3 inNormal : NORMAL;
    [[vk::location(2)]] float2 inUV     : TEXCOORD0;
};

struct VSOutput 
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPos          : WORLDPOS;
    [[vk::location(1)]] float3 normal            : NORMAL;
    [[vk::location(2)]] float2 uv                : TEXCOORD0;
    [[vk::location(3)]] float4 worldPosLightSpace: LIGHTSPACE;
    [[vk::location(4)]] nointerpolation uint matIndex : MATINDEX;
};

VSOutput main(VSInput input)
{
    float4x4 model = objBuf[pc.objIndex].model;
    float4 worldPos = mul(model, float4(input.inPos, 1.0));
    
    VSOutput o;
    o.worldPosLightSpace = mul(shadowMapUBO.lightViewProj, worldPos);
    o.position = mul(cam.proj, mul(cam.view, worldPos));
    o.worldPos = worldPos.xyz;
    o.normal   = normalize(mul((float3x3)model, input.inNormal));
    o.uv       = input.inUV;
    o.matIndex = pc.matIndex;
    
    return o;
}
