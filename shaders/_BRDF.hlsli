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
    float NdotV;
};

void FinalizeSurface(inout Surface s)
{
    s.NdotV = max(dot(s.N, s.V), 0.0f);
}

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

float SmithGGXVisibility(float NdotV, float NdotL, float alpha)
{
    float denom = lerp(2.0f * NdotL * NdotV, NdotL + NdotV, alpha);
    return 0.5f / max(denom, 1e-4f);
}

// Schlick's approximation, for direct/analytic lights
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    float x = clamp(1.0f - cosTheta, 0.0f, 1.0f);
    float x2 = x * x;
    float x5 = x2 * x2 * x; // strength-reduced pow(x, 5): kills the exp2/log2 pair
    return F0 + ((float3) 1.0f - F0) * x5;
}

// Roughness damped version for the split-sum IBL term only
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max((float3) (1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 EvaluateBRDF(Surface s, float3 L)
{
    float3 H = normalize(s.V + L);
    float NdotL = max(dot(s.N, L), 0.0f);
    float alpha = s.roughness * s.roughness; // GGX alpha

    float NDF = DistributionGGX(s.N, H, s.roughness);
    float Vis = SmithGGXVisibility(s.NdotV, NdotL, alpha);
    float3 F = FresnelSchlick(max(dot(H, s.V), 0.0f), s.f0);

    // kS is corresponded in F
    float3 kS = F;
    float3 kD = (float3) 1.0f - kS;
    // Metallic = Absorb refractance = No diffuse
    kD *= 1.0f - s.metallic;
    
    float3 specular = NDF * Vis * F;
    
    return kD * s.albedo / PI + specular;
}

#endif