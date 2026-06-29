#pragma once
#include <glm/glm.hpp>


// Lighting Data for Shaders
//TODO: add a shadow map index for all these light types. more robust than current (pure order based)
struct alignas(16) PointLightGPU
{
	glm::vec3 position;
	float intensity;
	glm::vec3 color;
	float range;
};

struct alignas(16) DirectionalLightGPU
{
	glm::vec3 direction;
	float intensity;
	glm::vec3 color;
	float range;
};

struct alignas(16) SpotLightGPU
{
	glm::vec3 position;
	float cutOff;
	glm::vec3 direction;
	float outerCutoff;
	glm::vec3 color;
	float intensity;
	float range;
};

// UBO, SSBO, PushConstant

struct alignas(16) CameraUBO
{
	glm::mat4 view;
	glm::mat4 proj;

	glm::vec3 eyePos;
	float nearPlane;	// for linearizing depth
	float farPlane;
};

struct alignas(16) LightUBO
{
	unsigned int pointLightNum;
	unsigned int dirLightNum;
	unsigned int spotLightNum;

	PointLightGPU pointLights[32];
	DirectionalLightGPU dirLights[4];
	SpotLightGPU spotLights[32];
};

struct alignas(16) SceneIBLUBO
{
	glm::vec3 ambientColor;
	float intensity;
	uint32_t irradianceIndex;
	uint32_t prefilteredIndex;
	uint32_t brdfLUTIndex;
	uint32_t samplerIndex;
	uint32_t prefilteredMaxMip;
};

struct alignas(16) ObjectSSBO
{
	glm::mat4 model;
	//Future TODO: normal, material index, AABB
};

struct alignas(16) MaterialSSBO
{
	uint32_t texIndices[4];
	uint32_t samplerIndices[4];
	glm::vec4 baseColorFactor;
	glm::vec3 ormFactor;
	float normalScale;
	glm::vec3 emissiveFactor;
};

struct PerDrawPC
{
	uint32_t objIndex;
	uint32_t matIndex;
};