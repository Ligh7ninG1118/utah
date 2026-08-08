#include "RenderSystem.h"

#include "VulkanRenderer.h"
#include "DrawJob.h"

#include <cstdio>

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
	_pDirLightPool = registry.GetPool<DirectionalLightCPU>();
	_pSpotLightPool = registry.GetPool<SpotLightCPU>();
	
	_pRenderer->Initialize();
}

void RenderSystem::Update(double deltaTime)
{
	GatherDrawList();

	GatherLights();

	_pRenderer->SetCameraComponent(_pCameraPool->GetPool()[0]);

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
	auto* meshManager = _pRenderer->GetMeshManager();

	_drawList.clear();
	_drawList.reserve(512);

	for (size_t i = 0; i < renderPool.size(); i++)
	{
		RenderComponent& rc = renderPool[i];
		const Model& model = meshManager->GetModel(rc._model);

		// raw model matrix
		const glm::mat4 placement = DrawJob::CalculatePlacement(_pTransformPool->Get(renderCompToEntity[i]));
		// intrinsic model matrix from asset
		const glm::mat4 finalModel = placement * model.intrinsicTransform;

		for (auto& prim : model.primitives)
		{
			DrawJob job;
			job._model = finalModel;
			job._minAABB = prim.minAABB;
			job._maxAABB = prim.maxAABB;
			job._vbHandle = model.vbHandle;
			job._ibHandle = model.ibHandle;
			job._firstIndex = prim.firstIndex;
			job._indexCount = prim.indexCount;
			job._vertexOffset = prim.vertexOffset;
			job._matIndex = rc._material.IsValid() ? rc._material.index : prim.matHandle.index;

			_drawList.push_back(job);
		}
	}

	FrustumCulling(_drawList, GenerateFrustum(_pCameraPool->GetPool()[0]));

	_pRenderer->SetDrawList(std::move(_drawList));
	_pRenderer->SetDebugAABBDrawList(std::move(_debugAABBDrawList));
}

void RenderSystem::GatherLights()
{
	auto& pointLightPool = _pPointLightPool->GetPool();
	auto& pointLightOwners = _pPointLightPool->GetEntities();

	std::vector<PointLightGPU> pointLights;
	pointLights.reserve(pointLightPool.size());

	for (size_t i = 0; i < pointLightPool.size(); i++)
	{
		const glm::vec3& pos = _pTransformPool->Get(pointLightOwners[i])._pos;
		pointLights.push_back(pointLightPool[i].ToGPU(pos));
	}

	auto& dirLightsPool = _pDirLightPool->GetPool();

	std::vector<DirectionalLightGPU> dirLights;
	dirLights.reserve(dirLightsPool.size());

	for (size_t i = 0; i < dirLightsPool.size(); i++)
	{
		dirLights.push_back(dirLightsPool[i].ToGPU());
	}

	auto& spotLightsPool = _pSpotLightPool->GetPool();
	auto& spotLightOwners = _pSpotLightPool->GetEntities();

	std::vector<SpotLightGPU> spotLights;
	spotLights.reserve(spotLightsPool.size());

	for (size_t i = 0; i < spotLightsPool.size(); i++)
	{
		const glm::vec3& pos = _pTransformPool->Get(spotLightOwners[i])._pos;
		spotLights.push_back(spotLightsPool[i].ToGPU(pos));
	}

	_pRenderer->SetLights(std::move(pointLights), std::move(dirLights), std::move(spotLights));
}

std::vector<Plane> RenderSystem::GenerateFrustum(const CameraComponent& cam) const
{
	std::vector<Plane> planes;
	planes.reserve(6);

	const float farDis = fabs(cam._farPlane);
	const float nearDis = fabs(cam._nearPlane);
	const float halfVSide = fabs(farDis * tan(glm::radians(cam._verticalFOV * 0.5f)));
	const float halfHSide = halfVSide * static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
	const glm::vec3 camPos = cam._pos;
	
	const glm::vec3 fwd = cam.GetFrontVector();
	const glm::vec3 rgt = cam.GetRightVector();
	const glm::vec3 up = glm::cross(rgt, fwd);

	Plane near;
	near._normal = glm::normalize(fwd);
	near._distance = glm::dot(-(camPos + nearDis * fwd), near._normal);
	planes.push_back(near);

	Plane far;
	far._normal = glm::normalize(-fwd);
	far._distance = glm::dot(-(camPos + farDis * fwd), far._normal);
	planes.push_back(far);

	Plane right;
	right._normal = glm::normalize(glm::cross(up, farDis * fwd + halfHSide * rgt));
	right._distance = glm::dot(-camPos, right._normal);
	planes.push_back(right);

	Plane left;
	left._normal = glm::normalize(glm::cross(farDis * fwd - halfHSide * rgt, up));
	left._distance = glm::dot(-camPos, left._normal);
	planes.push_back(left);

	Plane bottom;
	bottom._normal = glm::normalize(glm::cross(rgt, farDis * fwd - halfVSide * up));
	bottom._distance = glm::dot(-camPos, bottom._normal);
	planes.push_back(bottom);

	Plane top;
	top._normal = glm::normalize(glm::cross(farDis * fwd + halfVSide * up, rgt));
	top._distance = glm::dot(-camPos, top._normal);
	planes.push_back(top);

	return planes;
}

void RenderSystem::FrustumCulling(std::vector<DrawJob>& drawList, const std::vector<Plane>& frustum)
{
	//printf("Before culled: %d\n", drawList.size());

	std::vector<DrawJob> culledDrawList;

	culledDrawList.reserve(drawList.size());

	_debugAABBDrawList.clear();
	_debugAABBDrawList.reserve(drawList.size());

	for (uint32_t i = 0; i < drawList.size(); i++)
	{
		glm::vec3 minAABB = drawList[i]._minAABB;
		glm::vec3 maxAABB = drawList[i]._maxAABB;

		glm::vec3 centerLocal = (minAABB + maxAABB) * 0.5f;
		glm::vec3 halfExtentLocal = (maxAABB - minAABB) * 0.5f;

		glm::vec3 centerWorld = drawList[i]._model * glm::vec4(centerLocal, 1.0f);

		glm::mat3 absModel = glm::mat3(drawList[i]._model);
		absModel[0] = glm::abs(absModel[0]);
		absModel[1] = glm::abs(absModel[1]);
		absModel[2] = glm::abs(absModel[2]);

		glm::vec3 halfExtentWorld = absModel * halfExtentLocal;

		bool isWithin = true;
		for (const auto& plane : frustum)
		{
			float dist = glm::dot(plane._normal, centerWorld) + plane._distance;
			float radius = glm::dot(halfExtentWorld, glm::abs(plane._normal));

			if (dist + radius < 0.0f)
			{
				isWithin = false;
				break;
			}
		}

		if (isWithin)
		{
			culledDrawList.push_back(drawList[i]);
			glm::mat4 aabbModel = glm::mat4(1.0f);
			aabbModel = glm::translate(aabbModel, centerWorld);
			aabbModel = glm::scale(aabbModel, halfExtentWorld * 2.0f);
			_debugAABBDrawList.push_back(aabbModel);
		}
	}

	drawList = std::move(culledDrawList);

	//printf("After culled: %d\n", drawList.size());
}

void RenderSystem::NotifyResized()
{
	_pRenderer->NotifyResized();
}

MeshManager* RenderSystem::GetMeshManager()
{
	return _pRenderer->GetMeshManager();
}

MaterialManager* RenderSystem::GetMaterialManager()
{
	return _pRenderer->GetMaterialManager();
}


