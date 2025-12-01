#include "RenderSystem.h"

#include "VulkanRenderer.h"


RenderSystem::RenderSystem()
{
	//TODO: Use macro
	_pRenderer = new VulkanRenderer();
	_pRenderer->Initialize();
}

RenderSystem::~RenderSystem()
{
	delete _pRenderer;
}

void RenderSystem::Init(Registry& registry)
{
	_pTransformPool = registry.GetPool<TransformComponent>();
	_pRenderPool = registry.GetPool<RenderComponent>();
}

void RenderSystem::Update(float deltaTime)
{
	_pRenderer->DrawFrame();
}

void RenderSystem::WaitForRendererIdle()
{
	_pRenderer->WaitForIdle();
}

void RenderSystem::GatherDrawList()
{
}


