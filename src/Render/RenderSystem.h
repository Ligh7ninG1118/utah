#pragma once
#include "Core/System.h"
#include "Render/RenderComponent.h"
#include "Gameplay/TransformComponent.h"

class VulkanRenderer;

class RenderSystem : public System
{
public:
	RenderSystem();
	~RenderSystem();

	void Init(Registry& registry);

	void Update(double deltaTime) override;

	void WaitForRendererIdle();

	void GatherDrawList();

private:

	VulkanRenderer* _pRenderer;

	ComponentPool<TransformComponent>* _pTransformPool;
	ComponentPool<RenderComponent>* _pRenderPool;
};
