#pragma once
#include "Core/System.h"
#include "Render/RenderComponent.h"
#include "Gameplay/TransformComponent.h"

class IRenderer;

class RenderSystem : public System
{
public:
	RenderSystem();
	~RenderSystem();

	void Init(Registry& registry);

	void Update(float deltaTime) override;

	void WaitForRendererIdle();

	void GatherDrawList();

private:
	IRenderer* _pRenderer;

	ComponentPool<TransformComponent>* _pTransformPool;
	ComponentPool<RenderComponent>* _pRenderPool;
};
