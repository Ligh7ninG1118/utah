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
    float3 position;
    float constant;
    float3 ambient;
    float linear;
    float3 diffuse;
    float quadratic;
    float3 specular;
    float padding;
};

static const uint MAX_POINT_LIGHTS = 32;

struct LightUBO
{
    float3 eyePos;
    uint lightNum;

    PointLight pointLights[MAX_POINT_LIGHTS];
};

struct ObjectUBO
{
    float4x4 model;
};

float3 CalculatePointLightTex(PointLight light, float3 normal, float3 fragPos, float3 viewDir, float3 objColor)
{
    float distance = length(light.position - fragPos);
    float3 lightDir = normalize(light.position - fragPos);
    float attenuation = 1.0f / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    float3 ambient = light.ambient * objColor;
    ambient *= attenuation;

    float diffDot = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = light.diffuse * objColor * diffDot;
    diffuse *= attenuation;

    float3 H = normalize(lightDir + viewDir);
    float specDot = pow(max(dot(normal, H), 0.0f), 64.0f);
    float3 specular = light.specular * objColor * specDot;
    specular *= attenuation;

    return ambient + diffuse + specular;
}


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
    output.norm = normalize(mul((float3x3)objUBO.model, input.inNormal));
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
    float3 viewDir = normalize(lightUBO.eyePos - vertIn.worldPos);

    for (int i = 0; i < lightUBO.lightNum; i++)
    {
        result += CalculatePointLightTex(lightUBO.pointLights[i], vertIn.norm, vertIn.worldPos, viewDir, tex.xyz);
    }

    return result;
}