#include "_GlobalBindings.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	[[vk::location(0)]] float2 uv : TEXCOORD0;
};

static const float SAMPLE_RADIUS = 0.5f;
static const float DEPTH_BIAS = 0.025f;
static const uint KERNEL_SIZE = 64;

float main(PSInput input) : SV_TARGET
{
	// 0 is mipmap level
    int3 loadCoord = int3(input.position.xy, 0);
	
    float3 normalWS = gBufferColorTargets[G_BUFFER_COLOR_TARGET_NORMAL].Load(loadCoord).rgb;
	// sky pixels, early out
	if (dot(normalWS, normalWS) <= 1e-6f)
		return 1.0f;
	normalWS = normalize(normalWS);
	
	float fragRawDepth = depthTarget.Load(loadCoord).r;
    // Reconstruct world space position from depth
	// reverse z 
	float projA = cam.nearPlane / (cam.farPlane - cam.nearPlane);
	float projB = (cam.farPlane * cam.nearPlane) / (cam.farPlane - cam.nearPlane);
	float fragLinearDepth = projB / (fragRawDepth + projA);
	float2 ndc = input.uv * 2.0f - 1.0f;
	float3 viewRay = float3(ndc.x / cam.proj[0][0], ndc.y / cam.proj[1][1], -1.0f);
	float3 fragPosVS = viewRay * fragLinearDepth;
	float3 fragPosWS = mul(cam.invView, float4(fragPosVS, 1.0f)).xyz;
	
	// noise is vec2
	float3 rotationVec = float3(ssaoNoise.Load(int3(uint2(input.position.xy) & 3, 0)).rg, 0.0f);
	float3 tangent = normalize(rotationVec - normalWS * dot(rotationVec, normalWS));
	float3 bitangent = cross(normalWS, tangent);
	float3x3 TBN = float3x3(tangent, bitangent, normalWS);
	
	float occlusion = 0.0f;
	uint2 depthDim;
	depthTarget.GetDimensions(depthDim.x, depthDim.y);
	
	for (uint i = 0; i < KERNEL_SIZE; ++i)
	{
		// offset the position with kernel
		float3 sampleOffsetWS = mul(ssaoKernel.samples[i], TBN);
		float3 samplePosWS = fragPosWS + sampleOffsetWS * SAMPLE_RADIUS;
		float4 samplePosVS = mul(cam.view, float4(samplePosWS, 1.0f));
		float4 samplePosCS = mul(cam.proj, samplePosVS);
		
		// sample behind the eye
		if (samplePosCS.w <= 0.0f)
			continue;
		
		float2 sampleUV = (samplePosCS.xy / samplePosCS.w) * 0.5f + 0.5f;
		int2 sampleTexel = clamp(int2(sampleUV * float2(depthDim)), int2(0, 0), int2(depthDim) - 1);
		
		// get the depth at our new position's screen space coords, then linearize it back
		float sampledRawDepth = depthTarget.Load(int3(sampleTexel, 0)).r;
		float sampledLinearDepth = projB / (sampledRawDepth + projA);

		// range check to prevent far objects contribute to position's ao		
		float rangeCheck = smoothstep(0.0f, 1.0f, SAMPLE_RADIUS / abs(-samplePosVS.z - sampledLinearDepth));
		occlusion += (sampledLinearDepth <= -samplePosVS.z - DEPTH_BIAS ? 1.0f : 0.0f) * rangeCheck;
	}

	occlusion = 1.0f - (occlusion / KERNEL_SIZE);
	return occlusion;
}