#pragma once
#include <glm/glm.hpp>
#include "PointLightData.h"


struct PointLightComponent
{
	glm::vec3 ambient	{ 0.1f };
	glm::vec3 diffuse	{ 0.5f };
	glm::vec3 specular	{ 1.0f };

	float constant		= 1.0f;
	float linear		= 0.07f;
	float quadratic		= 0.017f;


	// Build struct for GPU layout
	PointLightData ToGPU(const glm::vec3& position) const
	{
		return PointLightData{
			position, constant,
			ambient, linear,
			diffuse, quadratic,
			specular, 0.0f // padding
		};
	}

};
