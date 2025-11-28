#include "RenderSystem.h"

#include "VulkanRenderer.h"


RenderSystem::RenderSystem()
{
	//TODO: Use macro
	_renderer = new VulkanRenderer();
	_renderer->Initialize();
}

RenderSystem::~RenderSystem()
{
	delete _renderer;
}

void RenderSystem::Update(float deltaTime)
{
	_renderer->DrawFrame();
}

void RenderSystem::WaitForRendererIdle()
{
	_renderer->WaitForIdle();
}


