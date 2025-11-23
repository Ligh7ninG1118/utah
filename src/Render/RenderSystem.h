#pragma once
#include "Core/System.h"

class IRenderer;

class RenderSystem : public System
{
public:
	RenderSystem();
	~RenderSystem();

	void Update(float deltaTime) override;

	void WaitForRendererIdle();

private:
	IRenderer* _renderer;

};
