#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include "Gameplay/TransformComponent.h"
#include "RenderCommons.h"

struct RenderComponent;

struct DrawJob
{
	glm::mat4 _model;
	glm::vec3 _minAABB;
	glm::vec3 _maxAABB;

	uint32_t _vbHandle = INVALID_HANDLE;
	uint32_t _ibHandle = INVALID_HANDLE;
	uint32_t _firstIndex = 0;
	uint32_t _indexCount = 0;
	int32_t  _vertexOffset = 0;
	uint32_t _matIndex = 0;

	static glm::mat4 CalculatePlacement(const TransformComponent& t)
	{
		glm::mat4 m(1.0f);
		m = glm::translate(m, t._pos);
		m = glm::rotate(m, glm::radians(t._rot.x), glm::vec3(1, 0, 0));
		m = glm::rotate(m, glm::radians(t._rot.y), glm::vec3(0, 1, 0));
		m = glm::rotate(m, glm::radians(t._rot.z), glm::vec3(0, 0, 1));
		m = glm::scale(m, t._scale);
		return m;
	}
};

