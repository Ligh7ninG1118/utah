#ifndef UTAH_SHADOW_MAPPING_HLSLI
#define UTAH_SHADOW_MAPPING_HLSLI
#pragma once

#include "_GlobalBindings.hlsli"

float ShadowCalculation(uint shadowIndex, float3 worldPos, float3 N, float3 L)
{
    float4 worldPosLightSpace = mul(shadowMap.lightViewProj[shadowIndex], float4(worldPos, 1.0f));
    
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

#endif