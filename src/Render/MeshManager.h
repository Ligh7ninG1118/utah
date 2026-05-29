#pragma once
#include "VulkanContext.h"
#include <glm/glm.hpp>
#include <vector>

class VulkanRenderer;

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

	uint32_t ImportMesh(const std::string& meshPath);

	const Mesh& GetMesh(uint32_t index) const { return _meshes[index]; }

private:
	VulkanRenderer* _pRenderer;

	std::vector<Mesh> _meshes;
};

