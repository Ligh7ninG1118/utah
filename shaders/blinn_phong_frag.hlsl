
struct PointLight
{
    float3 position;
    float attenConstant;
    float3 ambient;
    float attenLinear;
    float3 diffuse;
    float attenQuadratic;
    float3 specular;
    
    float padding;
};

// Mirror DirectionalLightGPU
struct DirectionalLight 
{
    float3 direction; float pad0;
    float3 ambient;   float pad1;
    float3 diffuse;   float pad2;
    float3 specular;  float pad3;
};

struct SpotLight 
{
    float3 position;  float attenConstant;
    float3 direction; float attenLinear;
    float3 ambient;   float attenQuadratic;
    float3 diffuse;   float cutoff;
    float3 specular;  float outerCutoff;
};

static const uint MAX_POINT_LIGHTS = 32;
static const uint MAX_DIR_LIGHTS   = 4;
static const uint MAX_SPOT_LIGHTS  = 32;

struct LightUBO 
{
    float3 eyePos;
    uint   pointLightNum;
    uint   dirLightNum;
    uint   spotLightNum;

    PointLight          pointLights[MAX_POINT_LIGHTS];
    DirectionalLight    dirLights[MAX_DIR_LIGHTS];
    SpotLight           spotLights[MAX_SPOT_LIGHTS];
};

struct MatData 
{
    uint   texIndices[4]; // [0] Albedo, [1] Specular
    float4 color;
    float  shininess;
};


[[vk::binding(1, 0)]] ConstantBuffer<LightUBO> lightUBO;

[[vk::binding(3, 0)]] StructuredBuffer<MatData> matBuf;

[[vk::binding(4, 0)]] Texture2D    textures[];
[[vk::binding(7, 0)]] SamplerState textureSamplers[];

[[vk::binding(6, 0)]] [[vk::combinedImageSampler]] Texture2D    shadowMap;
[[vk::binding(6, 0)]] [[vk::combinedImageSampler]] SamplerState shadowMapSampler;

struct PSInput 
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPos           : WORLDPOS;
    [[vk::location(1)]] float3 normal             : NORMAL;
    [[vk::location(2)]] float2 uv                 : TEXCOORD0;
    [[vk::location(3)]] float4 worldPosLightSpace : LIGHTSPACE;
    [[vk::location(4)]] nointerpolation uint matIndex : MATINDEX;
};

float3 CalculatePointLight(PointLight light, float3 normal, float3 fragPos, float3 viewDir,
                           float3 albedo, float specStrength, float shininess)
{
    float  dist     = length(light.position - fragPos);
    float3 lightDir = normalize(light.position - fragPos);
    float  atten    = 1.0 / (light.attenConstant + light.attenLinear * dist + light.attenQuadratic * dist * dist);

    float3 ambient  = light.ambient * albedo;
    float  diffDot  = max(dot(normal, lightDir), 0.0);
    float3 diffuse  = light.diffuse * albedo * diffDot;

    float3 H        = normalize(lightDir + viewDir);
    float  specDot  = pow(max(dot(normal, H), 0.0), shininess);
    float3 specular = light.specular * specStrength * specDot;

    return (ambient + diffuse + specular) * atten;
}

float3 CalculateDirectionalLight(DirectionalLight light, float3 normal, float3 viewDir,
                                 float3 albedo, float specStrength, float shininess)
{
    float3 lightDir = normalize(-light.direction);

    float3 ambient  = light.ambient * albedo;
    float  diffDot  = max(dot(normal, lightDir), 0.0);
    float3 diffuse  = light.diffuse * albedo * diffDot;

    float3 H        = normalize(lightDir + viewDir);
    float  specDot  = pow(max(dot(normal, H), 0.0), shininess);
    float3 specular = light.specular * specStrength * specDot;

    return ambient + diffuse + specular;
}

float3 CalculateSpotLight(SpotLight light, float3 normal, float3 fragPos, float3 viewDir,
                          float3 albedo, float specStrength, float shininess)
{
    float  dist     = length(light.position - fragPos);
    float  atten    = 1.0 / (light.attenConstant + light.attenLinear * dist + light.attenQuadratic * dist * dist);
    float3 lightDir = normalize(light.position - fragPos);

    float3 ambient  = light.ambient * albedo;
    float  diffDot  = max(dot(normal, lightDir), 0.0);
    float3 diffuse  = light.diffuse * albedo * diffDot;

    float3 H        = normalize(lightDir + viewDir);
    float  specDot  = pow(max(dot(normal, H), 0.0), shininess);
    float3 specular = light.specular * specStrength * specDot;

    float  theta     = dot(lightDir, normalize(-light.direction));
    float  intensity = clamp((theta - light.outerCutoff) / (light.cutoff - light.outerCutoff), 0.0, 1.0);
    diffuse  *= intensity;
    specular *= intensity;

    return (ambient + diffuse + specular) * atten;
}

float ShadowCalculation(float4 worldPosLightSpace)
{
    float3 projCoords = worldPosLightSpace.xyz / worldPosLightSpace.w;
    float2 uv = projCoords.xy * 0.5f + 0.5f;
    float currentDepth = projCoords.z;

    float closestDepth = shadowMap.Sample(shadowMapSampler, uv).r;
    float bias = 0.0f;
    float shadow = currentDepth < (closestDepth - bias) ? 0.5f : 0.0f; //reverse z
    return shadow;
}

float4 main(PSInput input) : SV_Target
{
    uint albedoIdx = matBuf[input.matIndex].texIndices[0];
    uint specIdx   = matBuf[input.matIndex].texIndices[1];
    float4 baseColor = matBuf[input.matIndex].color;
    float shininess = matBuf[input.matIndex].shininess;

    float4 texSample = textures[NonUniformResourceIndex(albedoIdx)].Sample(
                                textureSamplers[NonUniformResourceIndex(albedoIdx)], input.uv);
    float3 albedo = texSample.rgb * baseColor.rgb;
    float specStrength = textures[NonUniformResourceIndex(specIdx)].Sample(
                                textureSamplers[NonUniformResourceIndex(specIdx)], input.uv).r;

    float3 normal  = normalize(input.normal);
    float3 viewDir = normalize(lightUBO.eyePos - input.worldPos);
    float3 result  = float3(0.0, 0.0, 0.0);

    for (uint i = 0; i < lightUBO.pointLightNum; i++)
        result += CalculatePointLight(lightUBO.pointLights[i], normal, input.worldPos, viewDir, albedo, specStrength, shininess);

    for (uint j = 0; j < lightUBO.dirLightNum; j++)
        result += CalculateDirectionalLight(lightUBO.dirLights[j], normal, viewDir, albedo, specStrength, shininess);

    for (uint k = 0; k < lightUBO.spotLightNum; k++)
        result += CalculateSpotLight(lightUBO.spotLights[k], normal, input.worldPos, viewDir, albedo, specStrength, shininess);

    float shadow = ShadowCalculation(input.worldPosLightSpace);
    result *= (1.0 - shadow);

    return float4(result, texSample.a);
}
