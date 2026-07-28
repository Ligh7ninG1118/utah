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
    float4 Position : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Albedo : SV_Target2;
};


PSOutput main(PSInput input)
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
    
    
    PSOutput output = (PSOutput)0;
    
    output.Position = float4(input.worldPos, 1.0f);
    
    float3 N = normalize(input.normal);
    if (input.tangent.w != 0.0f)
    {
        float3 T = normalize(input.tangent.xyz);
        float3 B = cross(N, T) * input.tangent.w;
        float3 nTangent = normal * 2.0f - 1.0f;
        nTangent.xy *= mat.normalScale;
        N = normalize(T * nTangent.x + B * nTangent.y + N * nTangent.z);
    }
    
    output.Normal = float4(N, 1.0f);
    
    output.Albedo = float4(albedo, 1.0f);
    
    return output;
}
