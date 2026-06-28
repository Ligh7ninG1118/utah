#include "_GlobalBindings.hlsli"


struct VSInput
{
	[[vk::location(0)]] float3 inPos : POSITION;
};

struct VSOutput
{
	float4 position : SV_Position;
	[[vk::location(0)]] float3 localPos : LOCALPOS;
};


VSOutput main(VSInput input, uint ViewIndex : SV_ViewID)
{
	VSOutput o;
	o.localPos = input.inPos;
	o.position = mul(shadowMap.lightViewProj[ViewIndex], float4(input.inPos, 1.0f));
	return o;
}