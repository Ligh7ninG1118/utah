#pragma once
#include <glm/glm.hpp>

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