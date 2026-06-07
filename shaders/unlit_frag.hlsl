struct MatData 
{ 
    uint texIndices[4]; 
    uint samplerIndex[4];
    float4 color; 
    float shininess; 
};

[[vk::binding(3, 0)]] StructuredBuffer<MatData> matBuf;

struct PSInput 
{
    float4 position : SV_Position;
    [[vk::location(0)]] nointerpolation uint matIndex : MATINDEX;
};

float4 main(PSInput input) : SV_Target
{
    return matBuf[input.matIndex].color;
}
