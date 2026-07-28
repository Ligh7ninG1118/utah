#include "_GlobalBindings.hlsli"
#include "_BRDF.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

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