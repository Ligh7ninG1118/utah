struct VSInput 
{
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
    float4x4 outModel;
};

struct CameraUBO 
{
    float4x4 view;
    float4x4 proj;
};

struct ObjectUBO
{
    float4x4 model;
};

[[vk_binding(0, 0)]]
ConstantBuffer<CameraUBO> camUBO;
[[vk_binding(0, 1)]]
ConstantBuffer<ObjectUBO> objUBO;


[shader("vertex")]
VSOutput vertMain(VSInput input) 
{
    VSOutput output;

    output.pos = mul(camUBO.proj, mul(camUBO.view, mul(objUBO.model, float4(input.inPosition, 1.0))));
    output.outModel = objUBO.model;
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}
[[vk_binding(1, 1)]]
Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET 
{
   return texture.Sample(vertIn.fragTexCoord);
}