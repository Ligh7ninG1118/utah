#include "_GlobalBindings.hlsli"
#include "_Lighting.hlsli"
#include "_Clustering.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    int3 pixel = int3(input.position.xy, 0);
    //Texture2D::Load(x, y, miplevel)
    float3 normal = normalize(gBufferColorTargets[G_BUFFER_COLOR_TARGET_NORMAL].Load(pixel).rgb);
    float3 albedo = gBufferColorTargets[G_BUFFER_COLOR_TARGET_ALBEDO].Load(pixel).rgb;
    float3 orm = gBufferColorTargets[G_BUFFER_COLOR_TARGET_ORM].Load(pixel).rgb;
    float3 emissive = gBufferColorTargets[G_BUFFER_COLOR_TARGET_EMISSIVE].Load(pixel).rgb;
    
    float depth = depthTarget.Load(pixel).r;
    // reverse z 
    float projA = cam.nearPlane / (cam.farPlane - cam.nearPlane);
    float projB = (cam.farPlane * cam.nearPlane) / (cam.farPlane - cam.nearPlane);
    float linearDepth = projB/(depth + projA);
    
    float2 ndc = input.uv * 2.0f - 1.0f;
    float3 viewRay = float3(ndc.x / cam.proj[0][0], ndc.y / cam.proj[1][1], -1.0f);
  
    float3 viewPos = viewRay * linearDepth;
    float3 worldPos = mul(cam.invView, float4(viewPos, 1.0f)).xyz;
    
    Surface s;
    s.worldPos = worldPos;
    s.N = normal;
    s.V = normalize(cam.eyePos - worldPos);
    s.albedo = albedo;
    s.emissive = emissive;
    s.ao = min(orm.r, ao[AO_BLURRED].Load(pixel).r);
    s.roughness = orm.g;
    s.metallic = orm.b;
    // Assume base reflectivity to be at 0.04
    // And for metallic surface, lerp between F0 to albedo color
    s.f0 = lerp((float3) 0.04f, albedo, s.metallic);
    
    uint2 dims;
    depthTarget.GetDimensions(dims.x, dims.y);
    uint clusterKey = ClusterKeyFromDepth(depth, uint2(pixel.xy), dims);

    return float4(EvaluateIBL(s) + IntegrateLightsClustered(s, clusterKey) + s.emissive, 1.0f);
}