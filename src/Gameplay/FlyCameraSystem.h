#pragma once
#include "Core/System.h"
#include "Gameplay/CameraComponent.h"



class FlyCameraSystem : public System
{
public:
	FlyCameraSystem();
	~FlyCameraSystem();

	void Init(Registry& registry);

	void HandleMouseInput(double xpos, double ypos);
	void HandleKeyboardInput(int key, int scancode, int action, int mods);

private:
	ComponentPool<CameraComponent>* _pCameraPool;

	int _mainCamIndex = 0;

	double _lastMouseXPos;
	double _lastMouseYPos;
	float _mouseSens = 0.1f;

};

