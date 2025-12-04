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

void FlyCameraSystem::HandleMouseInput(double xpos, double ypos)
{
	CameraComponent& cam = _pCameraPool->GetPool()[_mainCamIndex];

	double deltaHor = _lastMouseXPos - xpos;
	double deltaVer = _lastMouseYPos - ypos;

	_lastMouseXPos = xpos;
	_lastMouseYPos = ypos;


	cam._rot.x += deltaVer * _mouseSens;
	cam._rot.y += -deltaHor * _mouseSens;

	cam._rot.y = glm::clamp(cam._rot.y, -89.0f, 89.0f);

	std::cout << cam._rot.y << std::endl;

}

void FlyCameraSystem::HandleKeyboardInput(int key, int scancode, int action, int mods)
{
	// this will not recognize hold!
	// bad, still need my own input system it seems
	glm::vec2 rawMove(0.0f, 0.0f);
	if (key == GLFW_KEY_W && action == GLFW_PRESS)
		rawMove.x = 1;
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
		rawMove.x = -1;
	if (key == GLFW_KEY_A && action == GLFW_PRESS)
		rawMove.y = -1;
	if (key == GLFW_KEY_D && action == GLFW_PRESS)
		rawMove.y = 1;
		
	if (glm::length(rawMove) >= glm::epsilon<float>())
	{
		rawMove = glm::normalize(rawMove);

		// polling rate?
		_pCameraPool->GetPool()[_mainCamIndex]._pos.x += rawMove.x * 0.05f;
		_pCameraPool->GetPool()[_mainCamIndex]._pos.y += rawMove.y * 0.05f;
	}
}
	

