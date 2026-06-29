#pragma once
#include <glm/glm.hpp>
#include "GPUTypes.h"

//TODO: Convert to PBR, only one color, one or no range limit

struct PointLightCPU
{
	glm::vec3 color{ 0.5f };
	float intensity = 1.0f;
	float range = 30.0f;

	// Build struct for GPU layout
	PointLightGPU ToGPU(const glm::vec3& position) const
	{
		return PointLightGPU{
			position, color, intensity, range
		};
	}
};

struct DirectionalLightCPU
{
	float pitch = -45.0f;
	float yaw = 0.0f;

	glm::vec3 color{ 0.5f };
	float intensity = 1.0f;
	float range = 30.0f;

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
			GetDirection(), color, intensity, range
		};
	}
};

struct SpotLightCPU
{
	float pitch = -45.0f;
	float yaw = 0.0f;

	float cutoff = 12.5f;
	float outerCutoff = 17.5f;

	glm::vec3 color;
	float intensity;
	float range;

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
			position, 
			GetDirection(),
			glm::cos(glm::radians(cutoff)),
			glm::cos(glm::radians(outerCutoff)),
			color, intensity, range
		};
	}
};