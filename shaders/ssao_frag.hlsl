#include "_GlobalBindings.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	[[vk::location(0)]] float2 uv : TEXCOORD0;
};

static const float2 noiseScale = float2(1920.0f / 4.0f, 1080.0f / 4.0f);
static const float radius = 0.5f;
static const float bias = 0.025f;

float main(PSInput input) : SV_TARGET
{
    int3 pixel = int3(input.position.xy, 0);
    float3 normal = normalize(gBufferColorTargets[G_BUFFER_COLOR_TARGET_NORMAL].Load(pixel).rgb);
	float depth = depthTarget.Load(pixel).r;
    // reverse z 
	float projA = cam.nearPlane / (cam.farPlane - cam.nearPlane);
	float projB = (cam.farPlane * cam.nearPlane) / (cam.farPlane - cam.nearPlane);
	float linearDepth = projB / (depth + projA);
	float2 ndc = input.uv * 2.0f - 1.0f;
	float3 viewRay = float3(ndc.x / cam.proj[0][0], ndc.y / cam.proj[1][1], -1.0f);
	float3 viewPos = viewRay * linearDepth;
	float3 worldPos = mul(cam.invView, float4(viewPos, 1.0f)).xyz;
	
	// noise is vec2
	float3 randomVec = float3(ssaoNoise.Sample(textureSamplers[SAMPLER_REPEAT_NEAREST], input.uv * noiseScale).rg, 0.0f);
	float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	float3 bitangent = cross(normal, tangent);
	float3x3 TBN = float3x3(tangent, bitangent, normal);
	
	float occlusion = 0.0f;
	
	for (uint i = 0; i < 64;++i)
	{
		float3 samplePos = mul(ssaoKernel.samples[i], TBN);
		samplePos = worldPos + samplePos * radius;
		
		float4 offset = float4(samplePos, 1.0f);
		float4 samplePosVS = mul(cam.view, float4(samplePos, 1.0f));
		offset = mul(cam.proj, mul(cam.view, offset));
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5f + 0.5f;
		
		//TODO: no linear here
		float sampleDepth = depthTarget.Sample(textureSamplers[SAMPLER_CLAMP_EDGE], offset.xy).r;
		sampleDepth = projB / (sampleDepth + projA);
		
		float rangeCheck = smoothstep(0.0f, 1.0f, radius / abs(-samplePosVS.z - sampleDepth));
		occlusion += (sampleDepth <= -samplePosVS.z - bias ? 1.0f : 0.0f) * rangeCheck;
	}
	
	occlusion = 1.0f - (occlusion / 64);
    
	return occlusion;
}