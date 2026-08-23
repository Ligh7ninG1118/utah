#include "_GlobalBindings.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

// outer and inner integrals. 12x slices currently, TODO on extend to use spatial + temporal reuse
static const uint SLICE_COUNT = 3;
static const uint STEPS_PER_SLICE = 4;
// world space, occlusion radius
static const float EFFECT_RADIUS = 0.5f;
// attenuation begins at
static const float FALLOFF_START = 0.6f;
// guard against near camera pixels
static const float MAX_RADIUS_UV = 0.15f;
static const float HALF_PI = 1.5707963f;

// Horizon search for one sample
float UpdateHorizon(float cHorizon, float2 sUV, float3 fragPosVS, float3 viewVec, uint2 screenDim, float projA, float projB)
{
    int2 texel = clamp(int2(sUV * float2(screenDim)), int2(0, 0), int2(screenDim) - 1);
    float rawDepth = depthTarget.Load(int3(texel, 0)).r;
    // reverse Z
    float linearDepth = projB / (rawDepth + projA);
    // XeGTAO, snaps to pixel center, ensure reconstruct xy at texel center, same as depth
    float2 snappedUV = (float2(texel) + 0.5f) / float2(screenDim);
    float2 sndc = snappedUV * 2.0f - 1.0f;
    float3 sPosVS = float3(sndc.x / cam.proj[0][0], sndc.y / cam.proj[1][1], -1.0f) * linearDepth;
    
    // horizon vector
    float3 hv = sPosVS - fragPosVS;
    float dist = length(hv);
    if (dist < 1e-4f)
        return cHorizon;
    
    // sample horizon cosine
    float shc = dot(hv / max(dist, 1e-5f), viewVec);
    // attenuation, linear from 0.6 * Radius -> Radius, then fade
    float falloff = saturate(1.0f - (dist / EFFECT_RADIUS - FALLOFF_START) / (1.0f - FALLOFF_START));

    return max(cHorizon, lerp(cHorizon, shc, falloff));
}


float main(PSInput input) : SV_TARGET
{
    int3 loadCoord = int3(input.position.xy, 0);
    
    float3 normalWS = gBufferColorTargets[G_BUFFER_COLOR_TARGET_NORMAL].Load(loadCoord).rgb;
    
    // sky early out
    if(dot(normalWS, normalWS) <= 1e-6f)
        return 1.0f;
    
    normalWS = normalize(normalWS);
    
    // Reconstruct world space position from depth
	// reverse z 
    float projA = cam.nearPlane / (cam.farPlane - cam.nearPlane);
    float projB = (cam.farPlane * cam.nearPlane) / (cam.farPlane - cam.nearPlane);
    float fragRawDepth = depthTarget.Load(loadCoord).r;
    float fragLinearDepth = projB / (fragRawDepth + projA);
    float2 ndc = input.uv * 2.0f - 1.0f;
    float3 viewRay = float3(ndc.x / cam.proj[0][0], ndc.y / cam.proj[1][1], -1.0f);
    float3 fragPosVS = viewRay * fragLinearDepth;
    
    float3 normalVS = normalize(mul((float3x3) cam.view, normalWS));
    float3 viewVec = normalize(-fragPosVS);
    
    uint2 screenDim;
    depthTarget.GetDimensions(screenDim.x, screenDim.y);
    
    float radiusUV = min(EFFECT_RADIUS * abs(cam.proj[1][1]) * 0.5f / fragLinearDepth, MAX_RADIUS_UV);
    
    float ign = frac(52.9829189f * frac(dot(input.position.xy, float2(0.06711056f, 0.00583715f))));
    float noiseStep = frac(ign * 1.6180339f);
    
    float visibility = 0.0f;
    
    for (uint slice = 0; slice < SLICE_COUNT; ++slice)
    {
        float phi = (PI * (float(slice) + ign)) / float(SLICE_COUNT);
        float2 omega = float2(cos(phi), sin(phi));
        
        const float3 directionVec = float3(omega.x, -omega.y, 0.0f);
        const float3 orthoDirectionVec = directionVec - (dot(directionVec, viewVec) * viewVec);
        const float3 axisVec = normalize(cross(orthoDirectionVec, viewVec));
        float3 projNormalVec = normalVS - axisVec * dot(normalVS, axisVec);
        
        float signNormal = (float) sign(dot(orthoDirectionVec, projNormalVec));
        float cosNormal = (float) saturate(dot(projNormalVec, viewVec) / max(length(projNormalVec), 1e-5f));
        float n = signNormal * acos(cosNormal);

        float cHorizonCos0 = cos(n + HALF_PI);
        float cHorizonCos1 = cos(n - HALF_PI);
        
        float slicePixelRadius = length(omega * radiusUV * float2(screenDim));
        float minS = 1.3f / max(slicePixelRadius, 1e-5f);

        for (uint step = 0; step < STEPS_PER_SLICE; ++step)
        {
            float s = (float(step) + noiseStep + 1.0f) / float(STEPS_PER_SLICE + 1);
            s = max(s, minS);
            float2 offUV = omega * (s * radiusUV);
            
            cHorizonCos0 = UpdateHorizon(cHorizonCos0, saturate(input.uv + offUV), fragPosVS, viewVec, screenDim, projA, projB);
            cHorizonCos1 = UpdateHorizon(cHorizonCos1, saturate(input.uv - offUV), fragPosVS, viewVec, screenDim, projA, projB);
        }
        
        float h0 = -acos(clamp(cHorizonCos1, -1.0f, 1.0f));
        float h1 = acos(clamp(cHorizonCos0, -1.0f, 1.0f));

        h0 = n + max(h0 - n, -HALF_PI);
        h1 = n + min(h1 - n, HALF_PI);

        float sinN = sin(n);
        float cosN = cos(n);
        
        float projLen = length(projNormalVec);
        float arc = 0.25f * ((2.0f * h0 * sinN - cos(2.0f * h0 - n) + cosN)
                            + (2.0f * h1 * sinN - cos(2.0f * h1 - n) + cosN));
        visibility += projLen * arc;
    }
    
    visibility = saturate(visibility / float(SLICE_COUNT));
    return visibility;
}