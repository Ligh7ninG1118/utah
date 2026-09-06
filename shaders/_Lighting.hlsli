#ifndef UTAH_LIGHTING_HLSLI
#define UTAH_LIGHTING_HLSLI
#pragma once

#include "_GlobalBindings.hlsli"
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

float3 ShadeDirLight(DirectionalLight l, Surface s)
{
    LightSample ls = SampleDirLight(l, s);
    float shadow = (l.shadowIndex != SHADOW_INDEX_NONE) ?
        ShadowCalculation(l.shadowIndex, s.worldPos, s.N, ls.L) : 0.0f;
    return EvaluateBRDF(s, ls.L) * ls.radiance * max(dot(s.N, ls.L), 0.0f) * (1.0f - shadow);
}

float3 ShadeSpotLight(SpotLight l, Surface s)
{
    LightSample ls = SampleSpotLight(l, s);
    float shadow = (l.shadowIndex != SHADOW_INDEX_NONE) ?
        ShadowCalculation(l.shadowIndex, s.worldPos, s.N, ls.L) : 0.0f;
    return EvaluateBRDF(s, ls.L) * ls.radiance * max(dot(s.N, ls.L), 0.0f) * (1.0f - shadow);
}

float3 ShadePointLight(PointLight l, Surface s)
{
    LightSample ls = SamplePointLight(l, s);
    float shadow = (l.shadowIndex != SHADOW_INDEX_NONE) ?
        ShadowCubeMapCalculation(l.shadowIndex, s.N, s.worldPos - l.position, cam.nearPlane, cam.farPlane) : 0.0f;
    return EvaluateBRDF(s, ls.L) * ls.radiance * max(dot(s.N, ls.L), 0.0f) * (1.0f - shadow);
}

void IntegrateDirSpot(Surface s, inout float3 Lo)
{
    for (uint j = 0; j < light.dirLightNum; j++)
        Lo += ShadeDirLight(light.dirLights[j], s);
    for (uint k = 0; k < light.spotLightNum; k++)
        Lo += ShadeSpotLight(light.spotLights[k], s);
}

float3 IntegrateLightsBrute(Surface s)
{
    float3 Lo = (float3) 0.0f;
    for (uint i = 0; i < light.pointLightNum; i++)
        Lo += ShadePointLight(pointLightBuf[i], s);
    IntegrateDirSpot(s, Lo);
    return Lo;
}

float3 IntegrateLightsClustered(Surface s, uint clusterKey)
{
    float3 Lo = (float3) 0.0f;
    if (clusterKey != INVALID_CLUSTER_KEY)
    {
        uint2 og = clusterLightGrid[clusterKey]; // (offset, count)
        for (uint n = 0; n < og.y; n++)
        {
            Lo += ShadePointLight(pointLightBuf[clusterLightList[0].list[og.x + n]], s);
        }
    }
    IntegrateDirSpot(s, Lo);
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