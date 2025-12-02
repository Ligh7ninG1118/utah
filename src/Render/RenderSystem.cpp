#include "RenderSystem.h"

#include "VulkanRenderer.h"
#include "DrawJob.h"

RenderSystem::RenderSystem()
{
	//TODO: Use macro
	_pRenderer = new VulkanRenderer();
}

RenderSystem::~RenderSystem()
{
	delete _pRenderer;
}

void RenderSystem::Init(Registry& registry)
{
	_pTransformPool = registry.GetPool<TransformComponent>();
	_pRenderPool = registry.GetPool<RenderComponent>();

	
	_pRenderer->Initialize(_pRenderPool->GetPool());
}

void RenderSystem::Update(float deltaTime)
{
	GatherDrawList();

	_pRenderer->DrawFrame();
}

void RenderSystem::WaitForRendererIdle()
{
	_pRenderer->WaitForIdle();
}

void RenderSystem::GatherDrawList()
{
	auto& renderPool = _pRenderPool->GetPool();
	auto& transformPool = _pTransformPool->GetPool();
	
	std::vector<DrawJob> list;
	list.reserve(renderPool.size());

	for (size_t i = 0; i < renderPool.size(); i++)
	{
		DrawJob job;
		job._renderComp = &renderPool[i];
		// Assuming same ordering
		job.SetModelMatrix(transformPool[i]);
		list.push_back(job);
	}

	_pRenderer->UpdateDrawList(list);
}


