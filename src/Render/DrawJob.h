#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include "Gameplay/TransformComponent.h"


struct RenderComponent;


struct DrawJob
{
	glm::mat4 _model;
	RenderComponent* _renderComp;

	void SetModelMatrix(const TransformComponent& transform)
	{
		_model = glm::mat4(1.0f);
		_model = glm::translate(_model, transform._pos);
	
		glm::quat qYaw = glm::angleAxis(transform._rot.y, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::quat qPitch = glm::angleAxis(transform._rot.x, glm::vec3(0.0f, 0.0f, 1.0f));
		glm::quat qRoll = glm::angleAxis(transform._rot.z, glm::vec3(1.0f, 0.0f, 0.0f));

		glm::quat q = qYaw * qPitch * qRoll;
		glm::mat4 R = glm::mat4(q);
		_model *= R;

		_model = glm::scale(_model, transform._scale);
	}
};

