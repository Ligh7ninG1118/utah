#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include "Core/TransformComponent.h"


struct RenderComponent;


struct DrawJob
{
	glm::mat4 _model;
	RenderComponent* _renderComp;

	glm::mat4 SetModelMatrix(const TransformComponent& transform)
	{
		_model = glm::mat4(1.0f);
		_model = glm::translate(_model, transform._pos);
		_model = glm::rotate(_model, transform._rot.x, glm::vec3(1.0f, 0.0f, 0.0f));
		_model = glm::rotate(_model, transform._rot.y, glm::vec3(0.0f, 1.0f, 0.0f));
		_model = glm::rotate(_model, transform._rot.z, glm::vec3(0.0f, 0.0f, 1.0f));
		_model = glm::scale(_model, transform._scale);
	}

};

