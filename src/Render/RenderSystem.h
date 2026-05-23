#pragma once
#include "Core/System.h"
#include "Render/RenderComponent.h"
#include "Render/CPUTypes.h"
#include "Gameplay/TransformComponent.h"
#include "Gameplay/CameraComponent.h"


class VulkanRenderer;
struct DrawJob;

class RenderSystem : public System
{
public:
	RenderSystem();
	~RenderSystem();

	void Init(Registry& registry);

	void Update(double deltaTime) override;

	void WaitForRendererIdle();

	void NotifyResized();

private:
	void GatherDrawList();

	void GatherLights();

	VulkanRenderer* _pRenderer;

	ComponentPool<TransformComponent>* _pTransformPool;
	ComponentPool<RenderComponent>* _pRenderPool;
	ComponentPool<CameraComponent>* _pCameraPool;
	ComponentPool<PointLightCPU>* _pPointLightPool;

	std::vector<DrawJob> _drawListScratch;
};
