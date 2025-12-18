#pragma once

#include <glm/glm.hpp>


struct alignas(16) PointLightData
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
