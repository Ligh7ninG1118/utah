#include "FlyCameraSystem.h"
#include "Core/UtahCtx.h"
#include <iostream>
#include <GLFW/glfw3.h>


FlyCameraSystem::FlyCameraSystem()
{

}

FlyCameraSystem::~FlyCameraSystem()
{
}

void FlyCameraSystem::Init(Registry& registry)
{
	_pCameraPool = registry.GetPool<CameraComponent>();
}

void FlyCameraSystem::Update(double deltaTime)
{
	GLFWwindow* window = UtahCtx::Get().GetContextWindow();
	glm::vec3 _rawMoveDir = glm::vec3(0.0f, 0.0f, 0.0f);
	if (window != nullptr)
	{
		const bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
		const bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
		const bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
		const bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
		const bool q = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
		const bool e = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;

		_rawMoveDir = glm::vec3(
			(w ? 1.0f : 0.0f) + (s ? -1.0f : 0.0f), // x (forward/back)
			(e ? 1.0f : 0.0f) + (q ? -1.0f : 0.0f), // y (up/down)
			(d ? 1.0f : 0.0f) + (a ? -1.0f : 0.0f)  // z (right/left)
		);
	}

	if (glm::length(_rawMoveDir) >= glm::epsilon<float>())
	{
		_rawMoveDir = glm::normalize(_rawMoveDir);

		CameraComponent& cam = _pCameraPool->GetPool()[_mainCamIndex];

		glm::vec3 moveDir = _rawMoveDir.x * cam.GetFrontVector() +
			_rawMoveDir.y * glm::vec3(0.0f, 1.0f, 0.0f) +
			_rawMoveDir.z * cam.GetRightVector();

		cam._pos += moveDir * 2.0f * static_cast<float>(deltaTime);
	}
}

void FlyCameraSystem::HandleMouseInput(double xpos, double ypos)
{
	CameraComponent& cam = _pCameraPool->GetPool()[_mainCamIndex];

	double deltaHor = xpos - _lastMouseXPos;
	double deltaVer = ypos - _lastMouseYPos;

	_lastMouseXPos = xpos;
	_lastMouseYPos = ypos;


	cam._rot.z += -deltaVer * _mouseSens;
	cam._rot.y += deltaHor * _mouseSens;

	cam._rot.z = glm::clamp(cam._rot.z, -89.0f, 89.0f);
}
	

