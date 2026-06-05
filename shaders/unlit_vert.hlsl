struct CameraUBO 
{ 
    float4x4 view; 
    float4x4 proj; 
};

struct ObjectData 
{ 
    float4x4 model; 
};

struct PushConstants 
{ 
    uint objIndex; 
    uint matIndex; 
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraUBO> cam;

[[vk::binding(2, 0)]] StructuredBuffer<ObjectData> objBuf;

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
