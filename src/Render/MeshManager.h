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

	void CreateAABBMesh();
	void CreatePyramidMesh(float baseSize = 1.0f, float height = 1.0f);
	void CreateIcosphereMesh(uint32_t subdivisions, float radius = 1.0f);

	uint32_t ImportMesh(const std::string& meshPath);

	const Mesh& GetMesh(uint32_t index) const { return _meshes[index]; }
	const Mesh& GetAABBMesh() const { return _aabbMesh; }
	const Mesh& GetPyramidMesh() const { return _pyramidMesh; }
	const Mesh& GetSphereMesh() const { return _sphereMesh; }

private:
	VulkanRenderer* _pRenderer;

	std::vector<Mesh> _meshes;


	// Pre-generated meshes for debug usages
	//TODO: Organize them in a better way?
	Mesh _aabbMesh;
	Mesh _pyramidMesh;
	Mesh _sphereMesh;
};

