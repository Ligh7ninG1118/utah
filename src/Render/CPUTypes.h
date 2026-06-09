#pragma once
#include <glm/glm.hpp>
#include "GPUTypes.h"

struct PointLightCPU
{
	glm::vec3 ambient{ 0.1f };
	glm::vec3 diffuse{ 0.5f };
	glm::vec3 specular{ 1.0f };

	float constant = 1.0f;
	float linear = 0.07f;
	float quadratic = 0.017f;

	// Build struct for GPU layout
	PointLightGPU ToGPU(const glm::vec3& position) const
	{
		return PointLightGPU{
			position, constant,
			ambient, linear,
			diffuse, quadratic,
			specular, 0.0f // padding
		};
	}
};

struct DirectionalLightCPU
{
	float pitch = -45.0f;
	float yaw = 0.0f;

	glm::vec3 ambient{ 0.1f };
	glm::vec3 diffuse{ 0.5f };
	glm::vec3 specular{ 1.0f };

	glm::vec3 GetDirection() const
	{
		float p = glm::radians(pitch);
		float y = glm::radians(yaw);
		return glm::normalize
		( glm::vec3(
				glm::cos(p) * glm::cos(y),
				glm::sin(p),
				glm::cos(p) * glm::sin(y))
		);
	}

	// Build struct for GPU layout
	DirectionalLightGPU ToGPU() const
	{
		return DirectionalLightGPU{
			GetDirection(), 0.0f, //paddings
			ambient,  0.0f,
			diffuse,  0.0f,
			specular, 0.0f
		};
	}
};

struct SpotLightCPU
{
	float pitch = -45.0f;
	float yaw = 0.0f;

	glm::vec3 ambient{ 0.1f };
	glm::vec3 diffuse{ 0.5f };
	glm::vec3 specular{ 1.0f };

	float constant = 1.0f;
	float linear = 0.07f;
	float quadratic = 0.017f;

	float cutoff = 12.5f;
	float outerCutoff = 17.5f;

	glm::vec3 GetDirection() const
	{
		float p = glm::radians(pitch);
		float y = glm::radians(yaw);
		return glm::normalize
		(glm::vec3(
			glm::cos(p) * glm::cos(y),
			glm::sin(p),
			glm::cos(p) * glm::sin(y))
		);
	}

	// Build struct for GPU layout
	SpotLightGPU ToGPU(const glm::vec3& position) const
	{
		return SpotLightGPU{
			position, constant,
			GetDirection(), linear,
			ambient, quadratic,
			diffuse, glm::cos(glm::radians(cutoff)),
			specular, glm::cos(glm::radians(outerCutoff))
		};
	}
};