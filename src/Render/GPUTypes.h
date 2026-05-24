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

// UBO, Object specific

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

struct alignas(16) ObjectGPU
{
	glm::mat4 model;
	//Future TODO: normal, material index, AABB
};

struct PerDrawPC
{
	uint32_t objectIndex;
	uint32_t textureIndex;
};