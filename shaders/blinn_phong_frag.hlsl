
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
    float3 direction;
    float pad0;
    float3 ambient;
    float pad1;
    float3 diffuse;
    float pad2;
    float3 specular;
    float pad3;
};

struct SpotLight
{
    float3 position;
    float attenConstant;
    float3 direction;
    float attenLinear;
    float3 ambient;
    float attenQuadratic;
    float3 diffuse;
    float cutoff;
    float3 specular;
    float outerCutoff;
};

static const uint MAX_POINT_LIGHTS = 32;
static const uint MAX_DIR_LIGHTS = 4;
static const uint MAX_SPOT_LIGHTS = 32;

struct LightUBO
{
    float3 eyePos;
    uint pointLightNum;
    uint dirLightNum;
    uint spotLightNum;

    PointLight pointLights[MAX_POINT_LIGHTS];
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
};

struct MatData
{
    uint texIndices[4]; // [0] Albedo, [1] Specular
    float4 color;
    float shininess;
};


[[vk::binding(1, 0)]] ConstantBuffer<LightUBO> lightUBO;

[[vk::binding(3, 0)]] StructuredBuffer<MatData> matBuf;

[[vk::binding(4, 0)]] Texture2D textures[];
[[vk::binding(7, 0)]] SamplerState textureSamplers[];

[[vk::binding(6, 0)]] [[vk::combinedImageSampler]] Texture2D shadowMap;
[[vk::binding(6, 0)]] [[vk::combinedImageSampler]] SamplerComparisonState shadowMapSampler;

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPos : WORLDPOS;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] float4 worldPosLightSpace : LIGHTSPACE;
    [[vk::location(4)]] nointerpolation uint matIndex : MATINDEX;
};

float3 CalculatePointLight(PointLight light, float3 normal, float3 fragPos, float3 viewDir,
                           float3 albedo, float specStrength, float shininess, float shadow)
{
    float dist = length(light.position - fragPos);
    float3 lightDir = normalize(light.position - fragPos);
    float atten = 1.0 / (light.attenConstant + light.attenLinear * dist + light.attenQuadratic * dist * dist);

    float3 ambient = light.ambient * albedo;
    float diffDot = max(dot(normal, lightDir), 0.0);
    float3 diffuse = light.diffuse * albedo * diffDot;

    float3 H = normalize(lightDir + viewDir);
    float specDot = pow(max(dot(normal, H), 0.0), shininess);
    float3 specular = light.specular * specStrength * specDot;

    return (ambient + (diffuse + specular) * (1.0f - shadow)) * atten;
}

float3 CalculateDirectionalLight(DirectionalLight light, float3 normal, float3 viewDir,
                                 float3 albedo, float specStrength, float shininess, float shadow)
{
    float3 lightDir = normalize(-light.direction);

    float3 ambient = light.ambient * albedo;
    float diffDot = max(dot(normal, lightDir), 0.0);
    float3 diffuse = light.diffuse * albedo * diffDot;

    float3 H = normalize(lightDir + viewDir);
    float specDot = pow(max(dot(normal, H), 0.0), shininess);
    float3 specular = light.specular * specStrength * specDot;

    return ambient + (diffuse + specular) * (1.0f - shadow);
}

float3 CalculateSpotLight(SpotLight light, float3 normal, float3 fragPos, float3 viewDir,
                          float3 albedo, float specStrength, float shininess, float shadow)
{
    float dist = length(light.position - fragPos);
    float atten = 1.0 / (light.attenConstant + light.attenLinear * dist + light.attenQuadratic * dist * dist);
    float3 lightDir = normalize(light.position - fragPos);

    float3 ambient = light.ambient * albedo;
    float diffDot = max(dot(normal, lightDir), 0.0);
    float3 diffuse = light.diffuse * albedo * diffDot;

    float3 H = normalize(lightDir + viewDir);
    float specDot = pow(max(dot(normal, H), 0.0), shininess);
    float3 specular = light.specular * specStrength * specDot;

    float theta = dot(lightDir, normalize(-light.direction));
    float intensity = clamp((theta - light.outerCutoff) / (light.cutoff - light.outerCutoff), 0.0, 1.0);
    diffuse *= intensity;
    specular *= intensity;

    return (ambient + (diffuse + specular) * (1.0f - shadow)) * atten;
}

float ShadowCalculation(float4 worldPosLightSpace, float3 N, float3 L, PSInput input)
{
    float3 projCoords = worldPosLightSpace.xyz / worldPosLightSpace.w;
    float currentDepth = projCoords.z;
    float2 uv = projCoords.xy * 0.5f + 0.5f;
    
    float NdotL = max(dot(N, L), 0.0f);
    float minBias = 0.000005f;
    float maxBias = 0.0001f;
    float bias = max(maxBias * (1.0f - NdotL), minBias);
    
    uint width, height;
    shadowMap.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    
    if (all(uint2(input.position.xy) == uint2(640, 360)))
        printf("w=%f z=%f ndotl=%f bias=%f uv=%f,%f texel=%f,%f\n",
           input.worldPosLightSpace.w, projCoords.z, NdotL, bias, uv.x, uv.y, texelSize.x, texelSize.y);
    
    // PCF
    float refDepth = currentDepth + bias;
    float sum = 0.0f;
    [unroll]
    for (float x = -1.5f; x <= 1.5f; x++)
    {
        for (float y = -1.5f; y <= 1.5f; y++)
        {
            float2 tempUV = uv + float2(x, y) * texelSize;
            float shadow = shadowMap.SampleCmpLevelZero(shadowMapSampler, tempUV, refDepth);
            sum += (1.0f - shadow);
        }

    }
    return sum / 16.0f;

    //float closestDepth = shadowMap.Sample(shadowMapSampler, uv).r;
    //float shadow = currentDepth < (closestDepth - bias) ? 1.0f : 0.0f; //reverse z
    //return shadow;
}

float4 main(PSInput input) : SV_Target
{
    uint albedoIdx = matBuf[input.matIndex].texIndices[0];
    uint specIdx = matBuf[input.matIndex].texIndices[1];
    float4 baseColor = matBuf[input.matIndex].color;
    float shininess = matBuf[input.matIndex].shininess;

    float4 texSample = textures[NonUniformResourceIndex(albedoIdx)].Sample(
                                textureSamplers[NonUniformResourceIndex(albedoIdx)], input.uv);
    float3 albedo = texSample.rgb * baseColor.rgb;
    float specStrength = textures[NonUniformResourceIndex(specIdx)].Sample(
                                textureSamplers[NonUniformResourceIndex(specIdx)], input.uv).r;

    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(lightUBO.eyePos - input.worldPos);
    float3 result = float3(0.0, 0.0, 0.0);
    float3 lightDir = normalize(-lightUBO.dirLights[0].direction);
    
    
    float shadow = ShadowCalculation(input.worldPosLightSpace, normal, lightDir, input);

    for (uint i = 0; i < lightUBO.pointLightNum; i++)
        result += CalculatePointLight(lightUBO.pointLights[i], normal, input.worldPos, viewDir, albedo, specStrength, shininess, shadow);

    for (uint j = 0; j < lightUBO.dirLightNum; j++)
        result += CalculateDirectionalLight(lightUBO.dirLights[j], normal, viewDir, albedo, specStrength, shininess, shadow);

    for (uint k = 0; k < lightUBO.spotLightNum; k++)
        result += CalculateSpotLight(lightUBO.spotLights[k], normal, input.worldPos, viewDir, albedo, specStrength, shininess, shadow);

    return float4(result, texSample.a);
}
