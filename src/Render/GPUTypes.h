#pragma once
#include <glm/glm.hpp>


// Lighting Data for Shaders

struct alignas(16) PointLightGPU
{
	glm::vec3 position;
	float constant;
	glm::vec3 ambient;
	float linear;
	glm::vec3 diffuse;
	float quadratic;
	glm::vec3 specular;
	float padding;
};

struct alignas(16) DirectionalLightGPU
{
	glm::vec3 direction; float padding0;
	glm::vec3 ambient; float padding1;
	glm::vec3 diffuse; float padding2;
	glm::vec3 specular; float padding3;
};

struct alignas(16) SpotLightGPU
{
	glm::vec3 position;
	float constant;
	glm::vec3 direction;
	float linear;
	glm::vec3 ambient;
	float quadratic;
	glm::vec3 diffuse;
	float cutOff;
	glm::vec3 specular;
	float outerCutoff;
};

// UBO, SSBO, PushConstant

struct alignas(16) CameraUBO
{
	glm::mat4 view;
	glm::mat4 proj;
};

struct alignas(16) LightUBO
{
	glm::vec3 eyePos;
	unsigned int pointLightNum;

	PointLightGPU pointLights[32];
};

struct alignas(16) ObjectSSBO
{
	glm::mat4 model;
	//Future TODO: normal, material index, AABB
};

struct alignas(16) MaterialSSBO
{
	uint32_t texIndices[4];
	glm::vec4 color;
};

struct PerDrawPC
{
	uint32_t objIndex;
	uint32_t matIndex;
};