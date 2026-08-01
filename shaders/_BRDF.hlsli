#ifndef UTAH_BRDF_HLSLI
#define UTAH_BRDF_HLSLI
#pragma once

#include "_SharedTypes.hlsli"

struct Surface
{
    float3 worldPos;
    float3 N;
    float3 V;
    float3 albedo;
    float3 f0;
    float3 emissive;
    float roughness;
    float metallic;
    float ao;
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

float3 EvaluateBRDF(Surface s, float3 L)
{
    L = normalize(L);
    float3 H = normalize(s.V + L);
    
    float NDF = DistributionGGX(s.N, H, s.roughness);
    float G = GeometrySmith(s.N, s.V, L, s.roughness);
    float3 F = FresnelSchlick(max(dot(H, s.V), 0.0f), s.f0, s.roughness);
    
    // kS is corresponded in F
    float3 kS = F;
    float3 kD = (float3) 1.0f - kS;
    // Metallic = Absorb refractance = No diffuse
    kD *= 1.0f - s.metallic;
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0f * max(dot(s.N, s.V), 0.0f) * max(dot(s.N, L), 0.0f) + 0.0001f;
    float3 specular = numerator / denominator;
    
    return kD * s.albedo / PI + specular;
}

#endif