#include "_GlobalBindings.hlsli"


struct PSInput 
{
    float4 position : SV_Position;
    [[vk::location(0)]] nointerpolation uint matIndex : MATINDEX;
};

float4 main(PSInput input) : SV_Target
{
    return matBuf[input.matIndex].color;
}
