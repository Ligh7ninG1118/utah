#include "_GlobalBindings.hlsli"


struct PushConstants
{
    uint envMapIndex;
    // obsolete, using static sampler index now
    uint samplerIndex;
    float roughness;
};

[[vk::push_constant]] PushConstants pc;

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 localPos : LOCALPOS;
};

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

float4 main(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.localPos);
    float3 R = N;
    float3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float3 prefilteredColor = (float3) 0.0f;
    float totalWeight = 0.0f;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, pc.roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0f);
        if (NdotL > 0.0f)
        {
            // source cubemap has no mip chain; sample mip 0 directly. some firefly noise at high roughness is acceptable.
            prefilteredColor += textureCubes[pc.envMapIndex].SampleLevel(textureSamplers[SAMPLER_CLAMP_EDGE], L, 0.0f).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;
    return float4(prefilteredColor, 1.0f);
}