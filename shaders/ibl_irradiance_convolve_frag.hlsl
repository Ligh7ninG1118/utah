#include "_GlobalBindings.hlsli"


struct PushConstants
{
    // reuse the same PC layout
	uint envMapIndex;
	uint samplerIndex;
};

[[vk::push_constant]] PushConstants pc;

struct PSInput
{
	float4 position : SV_Position;
	[[vk::location(0)]] float3 localPos : LOCALPOS;
};

float4 main(PSInput input) : SV_TARGET
{
	float3 normal = normalize(input.localPos);
	
	float3 irradiance = (float3) 0.0f;
	
	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));
	
	float sampleDelta = 0.025f;
	float nrSamples = 0.0f;
	
	// polar / yaw: 0 to 360 deg
	for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
	{
		// inclination / pitch: 0 to 90 deg
		for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
		{
			float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
			
			irradiance += textureCubes[pc.envMapIndex].Sample(textureSamplers[pc.samplerIndex], sampleVec).rgb * cos(theta) * sin(theta);
			nrSamples++;
		}
	}
	irradiance = PI * irradiance * (1.0f / float(nrSamples));
	return float4(irradiance, 1.0f);
}