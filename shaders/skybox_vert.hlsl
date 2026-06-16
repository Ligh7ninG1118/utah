struct CameraUBO
{
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraUBO> cam;

struct VSInput
{
    [[vk::location(0)]] float3 inPos : POSITION;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 uv : TEXCOORD0;
};


VSOutput main(VSInput input)
{
    VSOutput o;
    o.uv = input.inPos;
    
    float3x3 viewRot = (float3x3)cam.view;
    
    float4 pos = mul(cam.proj, float4(mul(viewRot, input.inPos), 1.0f));
    
    o.position = float4(pos.xy, 0.0f, pos.w);
    
    return o;
}