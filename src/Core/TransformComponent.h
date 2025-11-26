#pragma once
#include "Core/Component.h"

#include <glm/gtc/matrix_transform.hpp>


struct TransformComponent : public Component
{
	glm::vec3 _pos;
	glm::vec3 _rot;
	glm::vec3 _scale;
};