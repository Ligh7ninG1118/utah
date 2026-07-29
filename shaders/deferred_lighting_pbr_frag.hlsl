#include "_GlobalBindings.hlsli"
#include "_BRDF.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

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
    
    float depth = depthTarget.Load(int3(input.position.x, input.position.y, 0)).r;
    
    float3 V = normalize(cam.eyePos - position);
    
    float3 F0 = (float3) 0.04f;
    F0 = lerp(F0, albedo, metallic);
    
    float3 Lo = (float3) 0.0f;
    for (int i = 0; i < light.pointLightNum; i++)
    {
        float3 L = light.pointLights[i].position - position;
        float distance = length(L);
        L = normalize(L);
        float3 H = normalize(V + L);
        
        float attenuation = pow(saturate(1.0f - pow((distance / light.pointLights[i].range), 4)), 2) / (distance * distance + 0.00001f);
        float3 radiance = light.pointLights[i].color * light.pointLights[i].intensity * attenuation;
        
        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0, roughness);
        
        // kS is corresponded in F
        float3 kS = F;
        float3 kD = (float3) 1.0f - kS;
        // Metaliic = Absorb refractance = No diffuse
        kD *= 1.0f - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(normal, V), 0.0f) * max(dot(normal, L), 0.0f) + 0.0001f;
        float3 specular = numerator / denominator;
        
        float NdotL = max(dot(normal, L), 0.0f);
        //Needs the original Lighting vector to reconstruct depth
        float shadow = ShadowCubeMapCalculation(i, normal, -(light.pointLights[i].position - position), cam.nearPlane, cam.farPlane);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0f - shadow);
    }
    
    for (int i = 0; i < light.dirLightNum; i++)
    {
        float3 L = normalize(-light.dirLights[i].direction);
        float3 H = normalize(V + L);

        float3 radiance = light.dirLights[i].color * light.dirLights[i].intensity;

        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0, roughness);

        float3 kS = F;
        float3 kD = (float3) 1.0f - kS;
        kD *= 1.0f - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(normal, V), 0.0f) * max(dot(normal, L), 0.0f) + 0.0001f;
        float3 specular = numerator / denominator;

        float NdotL = max(dot(normal, L), 0.0f);
        float shadow = ShadowCalculation(i, position, normal, L);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0f - shadow);
    }

    for (int i = 0; i < light.spotLightNum; i++)
    {
        float3 L = light.spotLights[i].position - position;
        float distance = length(L);
        L = normalize(L);
        float3 H = normalize(V + L);

        float attenuation = 1.0f / (distance * distance);
        // cutoff / outerCutoff are pre-cos'd on CPU side (CPUTypes.h)
        float theta = dot(-L, normalize(light.spotLights[i].direction));
        float epsilon = light.spotLights[i].cutoff - light.spotLights[i].outerCutoff;
        float coneFalloff = saturate((theta - light.spotLights[i].outerCutoff) / epsilon);
        float3 radiance = light.spotLights[i].color * light.spotLights[i].intensity * attenuation * coneFalloff;

        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0, roughness);

        float3 kS = F;
        float3 kD = (float3) 1.0f - kS;
        kD *= 1.0f - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(normal, V), 0.0f) * max(dot(normal, L), 0.0f) + 0.0001f;
        float3 specular = numerator / denominator;

        float NdotL = max(dot(normal, L), 0.0f);
        float shadow = ShadowCalculation(light.dirLightNum + i, position, normal, L);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0f - shadow);
    }
    
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
    
    return float4(ambient + Lo + emissive, 1.0f);
}