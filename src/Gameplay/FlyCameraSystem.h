#pragma once
#include "Core/System.h"
#include "Gameplay/CameraComponent.h"



class FlyCameraSystem : public System
{
public:
	FlyCameraSystem();
	~FlyCameraSystem();

	void Init(Registry& registry);
	void Update(double deltaTime) override;
	void HandleMouseInput(double xpos, double ypos);
	void ToggleFastFlyMode() { _shoudldFlyFaster = !_shoudldFlyFaster; }

private:
	ComponentPool<CameraComponent>* _pCameraPool;

	int _mainCamIndex = 0;

	bool _firstTimeCapturingInput = true;
	double _lastMouseXPos;
	double _lastMouseYPos;
	float _mouseSens = 0.1f;

	float _defaultFlySpeed = 2.0f;
	float _fastFlySpeed = 5.0f;
	bool _shoudldFlyFaster = false;
};

