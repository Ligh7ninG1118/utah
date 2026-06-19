
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

static const float PI = 3.1415926535f;

struct LightUBO
{
    float3 eyePos;
    float nearPlane;
    float farPlane;
    uint pointLightNum;
    uint dirLightNum;
    uint spotLightNum;

    PointLight pointLights[MAX_POINT_LIGHTS];
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
};

struct MatData
{
    uint texIndices[4]; // [0] Albedo, [1] OMR, [2] Normal, [3] Emissive
    uint samplerIndices[4];
    float4 color;
    float shininess;
};

static const uint MAX_SHADOW_CASTER_LIGHTS = 64;

struct ShadowMapUBO
{
    float4x4 lightViewProj[MAX_SHADOW_CASTER_LIGHTS];
};


[[vk::binding(1, 0)]] ConstantBuffer<LightUBO> lightUBO;

[[vk::binding(3, 0)]] StructuredBuffer<MatData> matBuf;

[[vk::binding(4, 0)]] Texture2D textures[];
[[vk::binding(5, 0)]] SamplerState textureSamplers[];

[[vk::binding(6, 0)]] ConstantBuffer<ShadowMapUBO> shadowMapUBO;

[[vk::binding(7, 0)]] Texture2D shadowMaps[];
[[vk::binding(8, 0)]] SamplerComparisonState shadowMapCmpSampler;
[[vk::binding(9, 0)]] TextureCube shadowCubeMaps[];

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPos : WORLDPOS;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] nointerpolation uint matIndex : MATINDEX;
};


float DistributionGGX(float3 N, float3 H, float roughness)
{
    // Trowbridge-Reitz GGX
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    
    float nom = a2;
    float denom = NdotH2 * (a2 - 1.0f) + 1.0f;
    denom = PI * denom * denom;
    
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;
    
    float nom = NdotV;
    float denom = NdotV * (1.0f - k) + k;
    
    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}



float ShadowCalculation(uint shadowIndex, float3 worldPos, float3 N, float3 L)
{
    float4 worldPosLightSpace = mul(shadowMapUBO.lightViewProj[shadowIndex], float4(worldPos, 1.0f));
    
    float3 projCoords = worldPosLightSpace.xyz / worldPosLightSpace.w;
    float currentDepth = projCoords.z;
    float2 uv = projCoords.xy * 0.5f + 0.5f;
    
    // Bias to solve shadow acne, depends on angle
    float NdotL = max(dot(N, L), 0.0f);
    float minBias = 0.000005f;
    float maxBias = 0.0001f;
    float bias = max(maxBias * (1.0f - NdotL), minBias);
    
    uint width, height;
    shadowMaps[shadowIndex].GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    
    // PCF
    float refDepth = currentDepth + bias;
    float sum = 0.0f;
    for (float x = -1.5f; x <= 1.5f; x++)
    {
        for (float y = -1.5f; y <= 1.5f; y++)
        {
            float2 tempUV = uv + float2(x, y) * texelSize;
            float shadow = shadowMaps[shadowIndex].SampleCmpLevelZero(shadowMapCmpSampler, tempUV, refDepth);
            sum += (1.0f - shadow);
        }

    }
    
    return sum / 16.0f;
}

float ShadowCubeMapCalculation(uint shadowIndex, float3 N, float3 L, float nearPlane, float farPlane)
{
    float zE = max(max(abs(L.x), abs(L.y)), abs(L.z));
 
    // reconstruct linearize depth (can't use perspective result directly)
    float currentDepth = nearPlane / (nearPlane - farPlane) - (farPlane * nearPlane) / ((nearPlane - farPlane) * zE);
    
    float NdotL = max(dot(N, L), 0.0f);
    float minBias = 0.000005f;
    float maxBias = 0.0001f;
    float bias = max(maxBias * (1.0f - NdotL), minBias);

    float refDepth = currentDepth + bias;
    
    // PCF 
    static const float3 offsets[8] =
    {
        float3(1, 1, 1), float3(1, -1, 1), float3(-1, 1, 1), float3(-1, -1, 1),
        float3(1, 1, -1), float3(1, -1, -1), float3(-1, 1, -1), float3(-1, -1, -1)
    };
    
    float radius = length(L) * 0.001f;
    float sum = 0.0f;
    
    for (uint i = 0; i < 8; i++)
    {
        sum += (1.0 - shadowCubeMaps[shadowIndex].SampleCmpLevelZero(shadowMapCmpSampler, L + offsets[i] * radius, refDepth));
    }
    
    return sum / 8.0f;
}

float4 main(PSInput input) : SV_Target
{
    uint albedoIdx = matBuf[input.matIndex].texIndices[0];
    uint ormIdx = matBuf[input.matIndex].texIndices[1];
    uint normalIdx = matBuf[input.matIndex].texIndices[2];
    uint emissiveIdx = matBuf[input.matIndex].texIndices[3];
    
    uint albedoSamplerIdx = matBuf[input.matIndex].samplerIndices[0];
    uint ormSamplerIdx = matBuf[input.matIndex].samplerIndices[1];
    uint normalSamplerIdx = matBuf[input.matIndex].samplerIndices[2];
    uint emissiveSamplerIdx = matBuf[input.matIndex].samplerIndices[3];
    
    float3 albedo = textures[NonUniformResourceIndex(albedoIdx)].Sample(textureSamplers[albedoSamplerIdx], input.uv).rgb;
    float3 orm = textures[NonUniformResourceIndex(ormIdx)].Sample(textureSamplers[ormSamplerIdx], input.uv).rgb;
    float3 normal = textures[NonUniformResourceIndex(normalIdx)].Sample(textureSamplers[normalSamplerIdx], input.uv).rgb;
    float3 emissive = textures[NonUniformResourceIndex(emissiveIdx)].Sample(textureSamplers[emissiveSamplerIdx], input.uv).rgb;
    
    float ao = orm.r;
    float roughness = orm.g;
    float metallic = orm.b;
    
    float3 N = normalize(input.normal);
    float3 V = normalize(lightUBO.eyePos - input.worldPos);
    
    float3 F0 = (float3) 0.04f;
    F0 = lerp(F0, albedo, metallic);
    
    float3 Lo = (float3) 0.0f;
    for (int i = 0; i < lightUBO.pointLightNum; i++)
    {
        float3 L = lightUBO.pointLights[i].position - input.worldPos;
        float distance = length(L);
        L = normalize(L);
        float3 H = normalize(V + L);
        float attenuation = 1.0f / (distance * distance);
        float3 radiance = lightUBO.pointLights[i].diffuse;
        
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);
        
        float3 kS = F;
        float3 kD = (float3)1.0f - kS;
        kD *= 1.0f - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
        float3 specular = numerator / denominator;
        
        float NdotL = max(dot(N, L), 0.0f);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    float3 ambient = (float3) 0.03f * albedo * ao;
    float3 color = ambient + Lo + emissive; 
    
    color = color / (color + (float3) 1.0f);
    color = pow(color, (float3)(1.0f / 2.2f));
    
    return float4(color, 1.0f);
}
