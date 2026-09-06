#include "_GlobalBindings.hlsli"
#include "_Lighting.hlsli"
#include "_Clustering.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPos : WORLDPOS;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] nointerpolation uint matIndex : MATINDEX;
    [[vk::location(4)]] float4 tangent : TANGENT;
    
};

float4 main(PSInput input) : SV_Target
{
    MatData mat = matBuf[input.matIndex];
    
    uint albedoIdx = mat.texIndices[0];
    uint ormIdx = mat.texIndices[1];
    uint normalIdx = mat.texIndices[2];
    uint emissiveIdx = mat.texIndices[3];
    uint aoIdx = mat.texIndices[4];
    
    uint albedoSamplerIdx = mat.samplerIndices[0];
    uint ormSamplerIdx = mat.samplerIndices[1];
    uint normalSamplerIdx = mat.samplerIndices[2];
    uint emissiveSamplerIdx = mat.samplerIndices[3];
    uint aoSamplerIdx = mat.samplerIndices[4];
    
    float4 albedo = textures[NonUniformResourceIndex(albedoIdx)].Sample(textureSamplers[albedoSamplerIdx], input.uv);
    float3 orm = textures[NonUniformResourceIndex(ormIdx)].Sample(textureSamplers[ormSamplerIdx], input.uv).rgb;
    float3 normal = textures[NonUniformResourceIndex(normalIdx)].Sample(textureSamplers[normalSamplerIdx], input.uv).rgb;
    float3 emissive = textures[NonUniformResourceIndex(emissiveIdx)].Sample(textureSamplers[emissiveSamplerIdx], input.uv).rgb;
    float aoTex = textures[NonUniformResourceIndex(aoIdx)].Sample(textureSamplers[aoSamplerIdx], input.uv).r;
    
    float ao = lerp(1.0f, aoTex.r, mat.ormFactor.r);
    float roughness = orm.g * mat.ormFactor.g;
    float metallic = orm.b * mat.ormFactor.b;
   
    albedo *= mat.baseColorFactor;
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
    
    Surface s;
    s.worldPos = input.worldPos;
    s.N = N;
    s.V = normalize(cam.eyePos - input.worldPos);
    s.albedo = albedo.rgb;
    s.emissive = emissive;
    s.roughness = roughness;
    s.metallic = metallic;
    s.ao = ao;
    // Assume base reflectivity to be at 0.04
    // And for metallic surface, lerp between F0 to albedo color
    s.f0 = lerp((float3) 0.04f, albedo.rgb, metallic);
    
    uint2 dims;
    depthTarget.GetDimensions(dims.x, dims.y);
    uint clusterKey = ClusterKeyFromDepth(input.position.z, uint2(input.position.xy), dims);

    return float4(EvaluateIBL(s) + IntegrateLightsClustered(s, clusterKey) + s.emissive, albedo.a);
}
