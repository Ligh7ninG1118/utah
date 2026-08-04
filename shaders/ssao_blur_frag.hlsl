#include "_GlobalBindings.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

float main(PSInput input) : SV_TARGET
{
    uint2 ssaoDim;
    ssao[SSAO_RAW].GetDimensions(ssaoDim.x, ssaoDim.y);
    
    int2 center = int2(input.position.xy);
    int2 maxCoord = int2(ssaoDim) - 1;
    
    float2 texelSize = 1.0f / float2(ssaoDim);
    float result = 0.0f;
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            int2 tap = clamp(center + int2(x, y), int2(0, 0), maxCoord);
            result += ssao[SSAO_RAW].Load(int3(tap, 0)).r;
        }
    }
    return result / (4.0f * 4.0f);
}