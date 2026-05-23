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
	_pCameraPool = registry.GetPool<CameraComponent>();
	_pPointLightPool = registry.GetPool<PointLightCPU>();
	
	_pRenderer->Initialize();
}

void RenderSystem::Update(double deltaTime)
{
	GatherDrawList();

	GatherLights();

	_pRenderer->UpdateCamera(_pCameraPool->GetPool()[0]);

	_pRenderer->DrawFrame();
}

void RenderSystem::WaitForRendererIdle()
{
	_pRenderer->WaitForIdle();
}

void RenderSystem::GatherDrawList()
{
	auto& renderPool = _pRenderPool->GetPool();
	auto& renderCompToEntity = _pRenderPool->GetEntities();
	
	_drawListScratch.clear();
	_drawListScratch.reserve(renderPool.size());

	for (size_t i = 0; i < renderPool.size(); i++)
	{
		DrawJob job;
		job._renderComp = &renderPool[i];

		job.SetModelMatrix(_pTransformPool->Get(renderCompToEntity[i]));
		_drawListScratch.push_back(job);
	}

	_pRenderer->UpdateDrawList(std::move(_drawListScratch));
}

void RenderSystem::GatherLights()
{
	auto& pointLightPool = _pPointLightPool->GetPool();
	auto& owners = _pPointLightPool->GetEntities();

	std::vector<PointLightGPU> lights;
	lights.reserve(pointLightPool.size());

	for (size_t i = 0; i < pointLightPool.size(); i++)
	{
		const glm::vec3& pos = _pTransformPool->Get(owners[i])._pos;
		lights.push_back(pointLightPool[i].ToGPU(pos));
	}

	_pRenderer->UpdateLights(std::move(lights));
}

void RenderSystem::NotifyResized()
{
	_pRenderer->NotifyResized();
}



