#include "_GlobalBindings.hlsli"


struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPos : WORLDPOS;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] nointerpolation uint matIndex : MATINDEX;
    [[vk::location(4)]] float4 tangent : TANGENT;
    
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

float4 main(PSInput input) : SV_Target
{
    MatData mat = matBuf[input.matIndex];
    
    uint albedoIdx = mat.texIndices[0];
    uint ormIdx = mat.texIndices[1];
    uint normalIdx = mat.texIndices[2];
    uint emissiveIdx = mat.texIndices[3];
    
    uint albedoSamplerIdx = mat.samplerIndices[0];
    uint ormSamplerIdx = mat.samplerIndices[1];
    uint normalSamplerIdx = mat.samplerIndices[2];
    uint emissiveSamplerIdx = mat.samplerIndices[3];
    
    float3 albedo = textures[NonUniformResourceIndex(albedoIdx)].Sample(textureSamplers[albedoSamplerIdx], input.uv).rgb;
    float3 orm = textures[NonUniformResourceIndex(ormIdx)].Sample(textureSamplers[ormSamplerIdx], input.uv).rgb;
    float3 normal = textures[NonUniformResourceIndex(normalIdx)].Sample(textureSamplers[normalSamplerIdx], input.uv).rgb;
    float3 emissive = textures[NonUniformResourceIndex(emissiveIdx)].Sample(textureSamplers[emissiveSamplerIdx], input.uv).rgb;
    
    float ao = lerp(1.0f, orm.r, mat.ormFactor.r);
    float roughness = orm.g * mat.ormFactor.g;
    float metallic = orm.b * mat.ormFactor.b;
   
    albedo *= mat.baseColorFactor.rgb;
    emissive *= mat.emissiveFactor;
    
    float3 N = normalize(input.normal);
    if (input.tangent.w != 0.0f)
    {
        float3 T = normalize(input.tangent.xyz);
        float3 B = cross(N, T) * input.tangent.w;
        float3 nTangent = normal * 2.0f - 1.0f;
        nTangent.xy *= mat.normalScale;
        N = normalize(T * nTangent.x + B * nTangent.y + N * nTangent.z);
    }
    
    float3 V = normalize(cam.eyePos - input.worldPos);
    
    // Assume base reflectivity to be at 0.04
    // And for metallic surface, lerp between F0 to albedo color
    float3 F0 = (float3) 0.04f;
    F0 = lerp(F0, albedo, metallic);
    
    float3 Lo = (float3) 0.0f;
    for (int i = 0; i < light.pointLightNum; i++)
    {
        float3 L = light.pointLights[i].position - input.worldPos;
        float distance = length(L);
        L = normalize(L);
        float3 H = normalize(V + L);
        
        float attenuation = pow(saturate(1.0f - pow((distance / light.pointLights[i].range), 4)), 2) / (distance * distance + 0.00001f);
        float3 radiance = light.pointLights[i].color * light.pointLights[i].intensity * attenuation;
        
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0, roughness);
        
        // kS is corresponded in F
        float3 kS = F;
        float3 kD = (float3)1.0f - kS;
        // Metaliic = Absorb refractance = No diffuse
        kD *= 1.0f - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
        float3 specular = numerator / denominator;
        
        float NdotL = max(dot(N, L), 0.0f);
        float shadow = ShadowCubeMapCalculation(i, N, -L, cam.nearPlane, cam.farPlane);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    for (int i = 0; i < light.dirLightNum; i++)
    {
        float3 L = normalize(-light.dirLights[i].direction);
        float3 H = normalize(V + L);

        float3 radiance = light.dirLights[i].color * light.dirLights[i].intensity;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0, roughness);

        float3 kS = F;
        float3 kD = (float3) 1.0f - kS;
        kD *= 1.0f - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
        float3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0f);
        float shadow = ShadowCalculation(i, input.worldPos, N, L);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0f - shadow);
    }

    for (int i = 0; i < light.spotLightNum; i++)
    {
        float3 L = light.pointLights[i].position - input.worldPos;
        float distance = length(L);
        L = normalize(L);
        float3 H = normalize(V + L);

        float attenuation = 1.0f / (distance * distance);
        // cutoff / outerCutoff are pre-cos'd on CPU side (CPUTypes.h)
        float theta = dot(-L, normalize(light.spotLights[i].direction));
        float epsilon = light.spotLights[i].cutoff - light.spotLights[i].outerCutoff;
        float coneFalloff = saturate((theta - light.spotLights[i].outerCutoff) / epsilon);
        float3 radiance = light.spotLights[i].color * light.spotLights[i].intensity * attenuation * coneFalloff;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0, roughness);

        float3 kS = F;
        float3 kD = (float3) 1.0f - kS;
        kD *= 1.0f - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
        float3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0f);
        float shadow = ShadowCalculation(light.dirLightNum + i, input.worldPos, N, L);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0f - shadow);
    }
    
    float3 kS = FresnelSchlick(max(dot(N, V), 0.0f), F0, roughness);
    float3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;
    
    float3 irradiance = textureCubes[sceneIBL.irradianceIndex].Sample(textureSamplers[sceneIBL.samplerIndex], N).rgb;
    float3 diffuse = irradiance * albedo;
    
    float3 R = reflect(-V, N);
    float3 prefilteredColor = textureCubes[sceneIBL.prefilteredIndex].SampleLevel(textureSamplers[0], R, roughness * sceneIBL.prefilteredMaxMip).rgb;
    float2 brdf = textures[sceneIBL.brdfLUTIndex].Sample(textureSamplers[sceneIBL.samplerIndex], float2(max(dot(N, V), 0.0f), roughness)).rg;
    float3 specularIBL = prefilteredColor * (kS * brdf.r + brdf.g);
    
    float3 ambient = (kD * diffuse + specularIBL) * ao * sceneIBL.intensity + sceneIBL.ambientColor * albedo * ao;
    
    return float4(ambient + Lo + emissive, 1.0f);
}
