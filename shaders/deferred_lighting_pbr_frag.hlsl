#include "_GlobalBindings.hlsli"


struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

float DistributionGGX(float3 N, float3 H, float roughness)
{
    // Trowbridge-Reitz GGX
    // Disney trick: square the roughness for a better look
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
    // kDirectLighting
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

float3 FresnelSchlick(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max((float3) (1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float4 main(PSInput input) : SV_TARGET
{
    //Texture2D::Load(x, y, miplevel)
    float3 position = gBufferColorTargets[G_BUFFER_COLOR_TARGET_POSITION].Load(int3(input.position.x, input.position.y, 0)).rgb;
    float3 normal = gBufferColorTargets[G_BUFFER_COLOR_TARGET_NORMAL].Load(int3(input.position.x, input.position.y, 0)).rgb;
    float3 albedo = gBufferColorTargets[G_BUFFER_COLOR_TARGET_ALBEDO].Load(int3(input.position.x, input.position.y, 0)).rgb;
    float3 orm = gBufferColorTargets[G_BUFFER_COLOR_TARGET_ORM].Load(int3(input.position.x, input.position.y, 0)).rgb;
    float ao = orm.r;
    float roughness = orm.g;
    float metallic = orm.b;
    float3 emissive = gBufferColorTargets[G_BUFFER_COLOR_TARGET_EMISSIVE].Load(int3(input.position.x, input.position.y, 0)).rgb;
    
    float depth = gBufferDepthTarget.Load(int3(input.position.x, input.position.y, 0)).r;
    
    float3 V = normalize(cam.eyePos - position);
    
    float3 F0 = (float3) 0.04f;
    F0 = lerp(F0, albedo, metallic);
    
    float3 kS = FresnelSchlick(max(dot(normal, V), 0.0f), F0, roughness);
    float3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;
    
    float3 irradiance = textureCubes[sceneIBL.irradianceIndex].Sample(textureSamplers[SAMPLER_CLAMP_EDGE], normal).rgb;
    float3 diffuse = irradiance * albedo;
    
    float3 R = reflect(-V, normal);
    float3 prefilteredColor = textureCubes[sceneIBL.prefilteredIndex].SampleLevel(textureSamplers[SAMPLER_CLAMP_EDGE], R, roughness * sceneIBL.prefilteredMaxMip).rgb;
    float2 brdf = textures[sceneIBL.brdfLUTIndex].Sample(textureSamplers[SAMPLER_CLAMP_EDGE], float2(max(dot(normal, V), 0.0f), roughness)).rg;
    float3 specularIBL = prefilteredColor * (kS * brdf.r + brdf.g);
    
    float3 ambient = (kD * diffuse + specularIBL) * ao * sceneIBL.intensity + sceneIBL.ambientColor * albedo * ao;
    
    return float4(ambient + emissive, 1.0f);
}