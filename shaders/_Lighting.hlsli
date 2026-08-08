#ifndef UTAH_LIGHTING_HLSLI
#define UTAH_LIGHTING_HLSLI
#pragma once

#include "_BRDF.hlsli"
#include "_SharedTypes.hlsli"
#include "_ShadowMapping.hlsli"

struct LightSample
{
    float3 L;
    float3 radiance;
};

LightSample SampleDirLight(DirectionalLight l, Surface s)
{
    LightSample ls;
    ls.L = normalize(-l.direction);
    ls.radiance = l.color * l.intensity;
    return ls;
}

LightSample SampleSpotLight(SpotLight l, Surface s)
{
    float3 L = l.position - s.worldPos;
    float distance = length(L);
    L = normalize(L);
    
    float attenuation = 1.0f / (distance * distance + 0.00001f);
    
    float theta = dot(-L, normalize(l.direction));
    float epsilon = l.cutoff - l.outerCutoff;
    float coneFalloff = saturate((theta - l.outerCutoff) / epsilon);
    
    LightSample ls;
    ls.L = L;
    ls.radiance = l.color * l.intensity * attenuation * coneFalloff;
    return ls;
}

LightSample SamplePointLight(PointLight l, Surface s)
{
    float3 L = l.position - s.worldPos;
    float distance = length(L);
    L = normalize(L);
    
    float attenuation = pow(saturate(1.0f - pow((distance / l.range), 4)), 2) / (distance * distance + 0.00001f);
    
    LightSample ls;
    ls.L = L;
    ls.radiance = l.color * l.intensity * attenuation;
    return ls;
}

float3 IntegrateLights(Surface s)
{
    float3 Lo = (float3) 0.0f;
    
    for (uint i = 0; i < light.pointLightNum; i++)
    {
        LightSample ls = SamplePointLight(light.pointLights[i], s);
        int shadowIdx = light.pointLights[i].shadowIndex;
        float shadow = (shadowIdx != SHADOW_INDEX_NONE) ?
            ShadowCubeMapCalculation(shadowIdx, s.N, s.worldPos - light.pointLights[i].position, cam.nearPlane, cam.farPlane)
            : 0.0f;
        Lo += EvaluateBRDF(s, ls.L) * ls.radiance * max(dot(s.N, ls.L), 0.0f) * (1.0f - shadow);
    }
    
    for (uint i = 0; i < light.dirLightNum; i++)
    {
        LightSample ls = SampleDirLight(light.dirLights[i], s);
        int shadowIdx = light.dirLights[i].shadowIndex;
        float shadow = (shadowIdx != SHADOW_INDEX_NONE) ?
            ShadowCalculation(shadowIdx, s.worldPos, s.N, ls.L)
            : 0.0f;
        Lo += EvaluateBRDF(s, ls.L) * ls.radiance * max(dot(s.N, ls.L), 0.0f) * (1.0f - shadow);
    }
    
    for (uint i = 0; i < light.spotLightNum; i++)
    {
        LightSample ls = SampleSpotLight(light.spotLights[i], s);
        int shadowIdx = light.spotLights[i].shadowIndex;
        float shadow = (shadowIdx != SHADOW_INDEX_NONE) ?
            ShadowCalculation(shadowIdx, s.worldPos, s.N, ls.L)
            : 0.0f;
        Lo += EvaluateBRDF(s, ls.L) * ls.radiance * max(dot(s.N, ls.L), 0.0f) * (1.0f - shadow);
    }
    
    return Lo;
}

float3 EvaluateIBL(Surface s)
{
    float3 kS = FresnelSchlick(max(dot(s.N, s.V), 0.0f), s.f0, s.roughness);
    float3 kD = 1.0f - kS;
    kD *= 1.0f - s.metallic;
    
    float3 irradiance = textureCubes[sceneIBL.irradianceIndex].Sample(textureSamplers[SAMPLER_CLAMP_EDGE], s.N).rgb;
    float3 diffuse = irradiance * s.albedo;
    
    float3 R = reflect(-s.V, s.N);
    float3 prefilteredColor = textureCubes[sceneIBL.prefilteredIndex].SampleLevel(textureSamplers[SAMPLER_CLAMP_EDGE], R, s.roughness * sceneIBL.prefilteredMaxMip).rgb;
    float2 brdf = textures[sceneIBL.brdfLUTIndex].Sample(textureSamplers[SAMPLER_CLAMP_EDGE], float2(max(dot(s.N, s.V), 0.0f), s.roughness)).rg;
    float3 specularIBL = prefilteredColor * (kS * brdf.r + brdf.g);
    
    return (kD * diffuse + specularIBL) * s.ao * sceneIBL.intensity + sceneIBL.ambientColor * s.albedo * s.ao;
}

#endif