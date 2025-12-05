#pragma once
#include <glm/gtc/matrix_transform.hpp>


struct CameraComponent
{
	glm::vec3 _pos = { 0.0f, 0.0f, 0.0f };
	glm::vec3 _rot = { 0.0f, 0.0f, 0.0f };

	float _verticalFOV = 80.0f;
	float _nearPlane = 0.1f;
	float _farPlane = 1000.0f;

	//TODO: add this to transform too
	// y up and x forward
	glm::vec3 GetFrontVector() const
	{
		return glm::normalize(
			glm::vec3(
			glm::cos(glm::radians(_rot.z)) * glm::cos(glm::radians(_rot.y)),
			glm::sin(glm::radians(_rot.z)),
			glm::sin(glm::radians(_rot.y)) * glm::cos(glm::radians(_rot.z))
		));
	}

	glm::vec3 GetRightVector() const
	{
		return glm::normalize(glm::cross(GetFrontVector(), glm::vec3(0.0f, 1.0f, 0.0f)));
	}
};