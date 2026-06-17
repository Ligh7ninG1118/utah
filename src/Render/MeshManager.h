#pragma once
#include "VulkanContext.h"
#include "RenderCommons.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <string>

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

	void CreateUnitCubeMesh();
	void CreateAABBMesh();
	void CreatePyramidMesh(float baseSize = 1.0f, float height = 1.0f);
	void CreateIcosphereMesh(uint32_t subdivisions, float radius = 0.2f);
	void CreatePlane();

	MeshHandle ImportMeshOBJ(const std::string& meshPath, const std::string& name);
	MeshHandle ImportMeshGLTF(const std::string& meshPath, const std::string& name);

	[[nodiscard]] MeshHandle GetHandle(const std::string& name) const;

	[[nodiscard]] const Mesh& GetMesh(MeshHandle handle) const { return _meshes[handle.index]; }
	[[nodiscard]] const Mesh& GetDebugMesh(DebugMeshType type) { return _debugMeshes[static_cast<size_t>(type)]; }

private:
	MeshHandle RegisterName(const std::string& name);

	VulkanRenderer* _pRenderer;

	std::vector<Mesh> _meshes;
	// string name -> index/handle
	std::unordered_map<std::string, uint32_t> _nameMap;
	// Pre-generated meshes for debug usages
	std::array<Mesh, static_cast<size_t>(DebugMeshType::Count)> _debugMeshes;
};

