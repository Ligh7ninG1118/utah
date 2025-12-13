struct VSInput 
{
    float3 inPosition;
    float3 inNormal;
    float2 inTexCoord;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 norm;
    float3 worldPos;
    float2 fragTexCoord;
};

struct CameraUBO 
{
    float4x4 view;
    float4x4 proj;
};

struct PointLight
{
    float3 ambient;
    float3 diffuse;
    float3 specular;

    float constant;
    float linear;
    float quadratic;
}

static const uint MAX_POINT_LIGHTS = 32;

struct LightUBO
{
    float3 eyePos;
    uint lightNum;

    PointLight pointLights[MAX_POINT_LIGHTS];
}

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
    output.worldPos = mul(objUBO.model, float4(input.inPosition, 1.0)).xyz;
    output.pos = mul(camUBO.proj, mul(camUBO.view, float4(output.worldPos, 1.0)));
    // May need to use normalInterp (transpose(inverse(model))) if the model has non uniform scaling
    output.norm = input.inNormal;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[[vk_binding(1, 0)]]
ConstantBuffer<LightUBO> lightUBO;

[[vk_binding(1, 1)]]
Sampler2D texture;

[shader("fragment")]
float3 fragMain(VSOutput vertIn) : SV_TARGET
{
    float3 result = float3(0.0f);
    float4 tex = texture.Sample(vertIn.fragTexCoord);

    result += tex.xyz;
    result += lightUBO.pointLights[0].ambient;

    return result;
}