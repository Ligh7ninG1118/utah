#include "_GlobalBindings.hlsli"


struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
    [[vk::location(1)]] nointerpolation uint matIndex : MATINDEX;
};

void main(PSInput input)
{
    MatData mat = matBuf[input.matIndex];
    float alpha = textures[NonUniformResourceIndex(mat.texIndices[0])].Sample(textureSamplers[mat.samplerIndices[0]], input.uv).a
                * mat.baseColorFactor.a;
    clip(alpha - mat.params.g);
}
