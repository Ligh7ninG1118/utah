[[vk::binding(5, 0)]] SamplerState textureSamplers[];
[[vk::binding(12, 0)]] TextureCube skyboxCubemap;

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    return skyboxCubemap.Sample(textureSamplers[0], input.uv);
}