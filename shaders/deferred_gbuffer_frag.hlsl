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

struct PSOutput
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 ORM : SV_Target2;
    float4 Emissive : SV_Target3;
};


PSOutput main(PSInput input)
{
    MatData mat = matBuf[input.matIndex];
    
    uint albedoIdx = mat.texIndices[0];
    uint rmIdx = mat.texIndices[1];
    uint normalIdx = mat.texIndices[2];
    uint emissiveIdx = mat.texIndices[3];
    uint aoIdx = mat.texIndices[4];
    
    uint albedoSamplerIdx = mat.samplerIndices[0];
    uint rmSamplerIdx = mat.samplerIndices[1];
    uint normalSamplerIdx = mat.samplerIndices[2];
    uint emissiveSamplerIdx = mat.samplerIndices[3];
    uint aoSamplerIdx = mat.samplerIndices[4];
    
    float3 albedo = textures[NonUniformResourceIndex(albedoIdx)].Sample(textureSamplers[albedoSamplerIdx], input.uv).rgb;
    float3 rm = textures[NonUniformResourceIndex(rmIdx)].Sample(textureSamplers[rmSamplerIdx], input.uv).rgb;
    float3 normal = textures[NonUniformResourceIndex(normalIdx)].Sample(textureSamplers[normalSamplerIdx], input.uv).rgb;
    float3 emissive = textures[NonUniformResourceIndex(emissiveIdx)].Sample(textureSamplers[emissiveSamplerIdx], input.uv).rgb;
    float aoTex = textures[NonUniformResourceIndex(aoIdx)].Sample(textureSamplers[aoSamplerIdx], input.uv).r;
    
    float ao = lerp(1.0f, aoTex.r, mat.ormFactor.r);
    float roughness = rm.g * mat.ormFactor.g;
    float metallic = rm.b * mat.ormFactor.b;
   
    albedo *= mat.baseColorFactor.rgb;
    emissive *= mat.emissiveFactor.rgb;

    float3 N = normalize(input.normal);
    if (input.tangent.w != 0.0f)
    {
        float3 T = normalize(input.tangent.xyz);
        float3 B = cross(N, T) * input.tangent.w;
        float3 nTangent = normal * 2.0f - 1.0f;
        nTangent.xy *= mat.params.x; // multiply with normal scale
        N = normalize(T * nTangent.x + B * nTangent.y + N * nTangent.z);
    }
    
    PSOutput output = (PSOutput) 0;
    output.Normal = float4(N, 1.0f);
    output.Albedo = float4(albedo, 1.0f);
    output.ORM = float4(ao, roughness, metallic, 1.0f);
    output.Emissive = float4(emissive, 1.0f);
    
    return output;
}
