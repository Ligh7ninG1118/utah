#include "_GlobalBindings.hlsli"


struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

float3 LinearToSRGB(float3 c)
{
    float3 lo = c * 12.92f;
    float3 hi = 1.055f * pow(max(c, 0.0f), 1.0f / 2.4f) - 0.055f;
    return lerp(hi, lo, step(c, (float3) 0.0031308f));
}

float4 main(PSInput input) : SV_TARGET
{
    const float startCompression = 0.8f - 0.04f;
    const float desaturation = 0.15f;
    float3 color = hdrTexture.Sample(hdrSampler, input.uv).rgb;
    
    float x = min(color.r, min(color.g, color.b));
    float offset = x <= 0.08f ? x - 6.25f * x * x : 0.04f;
    color -= offset;
    
    float peak = max(color.r, max(color.g, color.b));
    if (peak >= startCompression)
    {
        const float d = 1.0f - startCompression;
        float newPeak = 1.0f - d * d / (peak + d - startCompression);
        color *= newPeak / peak;
        float g = 1.0f - 1.0f / (desaturation * (peak - newPeak) + 1.0f);
        color = lerp(color, newPeak * float3(1.0f, 1.0f, 1.0f), g);
    }
    
    return float4(LinearToSRGB(color), 1.0f);
}

// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/pbrNeutral.glsl