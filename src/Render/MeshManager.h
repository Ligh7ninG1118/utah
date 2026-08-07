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

struct Primitive
{
	uint32_t indexCount = 0;
	uint32_t firstIndex = 0;
	int32_t vertexOffset = 0;
	MaterialHandle matHandle;
	glm::vec3 maxAABB{};
	glm::vec3 minAABB{};
};

struct Model
{
	uint32_t vbHandle;
	uint32_t ibHandle;
	vk::IndexType indexType;
	std::vector<Primitive> primitives;
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

	ModelHandle ImportMeshOBJ(const std::string& meshPath, const std::string& name);
	ModelHandle ImportModelGLTF(const std::string& meshPath, const std::string& name);

	[[nodiscard]] ModelHandle GetHandle(const std::string& name) const;

	[[nodiscard]] const Model& GetModel(ModelHandle handle) const { return _models[handle.index]; }
	[[nodiscard]] const Mesh& GetDebugMesh(DebugMeshType type) { return _debugMeshes[static_cast<size_t>(type)]; }

	[[nodiscard]] AllocatedBuffer& GetBuffer(uint32_t handle) { return _bufferPool[handle]; }

private:
	ModelHandle RegisterName(const std::string& name);
	[[nodiscard]] uint32_t AddBufferToPool(AllocatedBuffer buffer);

	VulkanRenderer* _pRenderer;

	std::vector<Model> _models;
	// string name -> index/handle
	std::unordered_map<std::string, uint32_t> _nameMap;
	// Pre-generated meshes for debug usages
	std::array<Mesh, static_cast<size_t>(DebugMeshType::Count)> _debugMeshes;

	std::vector<AllocatedBuffer> _bufferPool;
};

