#pragma once
#include "VulkanContext.h"
#include <vector>

class VulkanRenderer;

struct Mesh
{
	AllocatedBuffer vertexBuffer{};
	AllocatedBuffer indexBuffer{};
	uint32_t indexCount = 0;
};

class MeshManager
{
public:
	MeshManager();
	~MeshManager();

	void Initialize(VulkanRenderer* rendererRef);

	uint32_t ImportMesh(const std::string& meshPath);

	Mesh GetMesh(uint32_t index) const { return _meshes[index]; }

private:
	VulkanRenderer* _pRenderer;

	std::vector<Mesh> _meshes;
};

