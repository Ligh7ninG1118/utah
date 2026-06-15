
[[vk::binding(10, 0)]] Texture2D hdrTexture;
[[vk::binding(11, 0)]] SamplerState hdrSampler;

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};


float4 main(PSInput input) : SV_TARGET
{
    const float startCompression = 0.8f - 0.04f;
    const float desaturation = 0.15f;
    float3 color = hdrTexture.Sample(hdrSampler, input.uv).rgb;
    
    float x = min(color.r, min(color.g, color.b));
    float offset = x <= 0.08f ? x - 6.25f * x * x : 0.04f;
    color -= offset;
    
    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression)
        return float4(color, 1.0f);
    
    const float d = 1.0f - startCompression;
    float newPeak = 1.0f - d * d / (peak + d - startCompression);
    color *= newPeak / peak;
    float g = 1.0f - 1.0f / (desaturation * (peak - newPeak) + 1.0f);
    float3 colorOut = lerp(color, newPeak * float3(1.0f, 1.0f, 1.0f), g);
    return float4(colorOut, 1.0f);
}

// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/pbrNeutral.glsl