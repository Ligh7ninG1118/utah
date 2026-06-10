#pragma once
#include "Core/System.h"
#include "Render/RenderComponent.h"
#include "Render/CPUTypes.h"
#include "Gameplay/TransformComponent.h"
#include "Gameplay/CameraComponent.h"
#include "RenderCommons.h"


class VulkanRenderer;
class MeshManager;
class MaterialManager;
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

	MeshManager* GetMeshManager();
	MaterialManager* GetMaterialManager();

private:
	void GatherDrawList();

	void GatherLights();

	void FrustumCulling(std::vector<DrawJob>& drawList, const std::vector<Plane>& frustum);

	std::vector<Plane> GenerateFrustum(const CameraComponent& cam) const;

	VulkanRenderer* _pRenderer;

	ComponentPool<TransformComponent>* _pTransformPool;
	ComponentPool<RenderComponent>* _pRenderPool;
	ComponentPool<CameraComponent>* _pCameraPool;
	ComponentPool<PointLightCPU>* _pPointLightPool;
	ComponentPool<DirectionalLightCPU>* _pDirLightPool;
	ComponentPool<SpotLightCPU>* _pSpotLightPool;

	std::vector<DrawJob> _drawList;
	std::vector<glm::mat4> _debugAABBDrawList;
};
