#pragma once
#include "Core/System.h"
#include "Render/RenderComponent.h"

class IRenderer;

class RenderSystem : public System
{
public:
	RenderSystem();
	~RenderSystem();

	void Update(float deltaTime) override;

	void WaitForRendererIdle();

	void GatherDrawList();

private:
	IRenderer* _renderer;



};
