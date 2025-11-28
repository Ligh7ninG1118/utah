#pragma once
#include "Core/Component.h"

#include <glm/gtc/matrix_transform.hpp>


struct TransformComponent : Component
{
	glm::vec3 _pos = { 0.0f, 0.0f, 0.0f };
	glm::vec3 _rot = { 0.0f, 0.0f, 0.0f };
	glm::vec3 _scale = { 1.0f, 1.0f, 1.0f };
};