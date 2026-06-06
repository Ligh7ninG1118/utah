#pragma once
#include "VulkanContext.h"
#include <glm/glm.hpp>
#include <vector>

class VulkanRenderer;

enum class DebugMeshType : uint32_t
{
	AABB = 0,
	Pyramid,
	Icosphere,
	Count
};

struct Mesh
{
	AllocatedBuffer vertexBuffer{};
	AllocatedBuffer indexBuffer{};
	glm::vec3 maxAABB{};
	glm::vec3 minAABB{};
	uint32_t indexCount = 0;
};

class MeshManager
{
public:
	MeshManager();
	~MeshManager();

	void Initialize(VulkanRenderer* rendererRef);

	void CreateAABBMesh();
	void CreatePyramidMesh(float baseSize = 1.0f, float height = 1.0f);
	void CreateIcosphereMesh(uint32_t subdivisions, float radius = 1.0f);
	void CreatePlane();

	uint32_t ImportMesh(const std::string& meshPath);

	const Mesh& GetMesh(uint32_t index) const { return _meshes[index]; }
	const Mesh& GetDebugMesh(DebugMeshType type) { return _debugMeshes[static_cast<size_t>(type)]; }

private:
	VulkanRenderer* _pRenderer;

	std::vector<Mesh> _meshes;
	// Pre-generated meshes for debug usages
	std::array<Mesh, static_cast<size_t>(DebugMeshType::Count)> _debugMeshes;
};

