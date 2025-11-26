struct VSInput 
{
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct CameraUBO 
{
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<CameraUBO> camUBO;

//struct ObjectUBO
//{
//    float4x4 model;
//};
//ConstantBuffer<ObjectUBO> objUBO;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) 
{
    VSOutput output;
    float4x4 fakeModel = { { 1.0f, 0.0f, 0.0f, 0.0f },
                           { 0.0f, 1.0f, 0.0f, 0.0f },
                            { 0.0f, 0.0f, 1.0f, 0.0f },
                            { 0.0f, 0.0f, 0.0f, 1.0f } };

    output.pos = mul(camUBO.proj, mul(camUBO.view, mul(fakeModel, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET 
{
   return texture.Sample(vertIn.fragTexCoord);
}