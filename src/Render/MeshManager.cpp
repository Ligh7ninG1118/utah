#include "MeshManager.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>

#include "VulkanRenderer.h"

MeshManager::MeshManager()
{
}

MeshManager::~MeshManager()
{
	for (auto& mesh : _meshes)
	{
		_pRenderer->DestroyBuffer(mesh.vertexBuffer);
		_pRenderer->DestroyBuffer(mesh.indexBuffer);
	}
}

void MeshManager::Initialize(VulkanRenderer* renderer)
{
	_pRenderer = renderer;
}

uint32_t MeshManager::ImportMesh(const std::string& meshPath)
{
	tinyobj::attrib_t                attrib;
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      err;
	std::string                      warn;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, meshPath.c_str()))
	{
		throw std::runtime_error(err);
		return 0xFFFFFFFF;
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	std::vector<Vertex>    vertices;
	std::vector<uint32_t>  indices;

	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};

			vertex.pos = { attrib.vertices[3 * index.vertex_index + 0],
							attrib.vertices[3 * index.vertex_index + 1],
							attrib.vertices[3 * index.vertex_index + 2] };

			vertex.normal = { attrib.normals[3 * index.normal_index + 0],
							attrib.normals[3 * index.normal_index + 1],
							attrib.normals[3 * index.normal_index + 2] };

			vertex.texCoord = { attrib.texcoords[2 * index.texcoord_index + 0],
							   1.0f - attrib.texcoords[2 * index.texcoord_index + 1] };

			if (!uniqueVertices.contains(vertex))
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}


	// Create Vertex Buffer
	vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	AllocatedBuffer stagingBuffer = _pRenderer->CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

	memcpy(stagingBuffer.info.pMappedData, vertices.data(), bufferSize);

	AllocatedBuffer vertexBuffer = _pRenderer->CreateBuffer(bufferSize,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		VMA_MEMORY_USAGE_AUTO);

	_pRenderer->CopyBuffer(stagingBuffer.buffer, vertexBuffer.buffer, bufferSize);

	_pRenderer->DestroyBuffer(stagingBuffer);


	// Create Index Buffer
	bufferSize = sizeof(indices[0]) * indices.size();

	stagingBuffer = _pRenderer->CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

	memcpy(stagingBuffer.info.pMappedData, indices.data(), bufferSize);

	AllocatedBuffer indexBuffer = _pRenderer->CreateBuffer(bufferSize,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		VMA_MEMORY_USAGE_AUTO);

	_pRenderer->CopyBuffer(stagingBuffer.buffer, indexBuffer.buffer, bufferSize);

	_pRenderer->DestroyBuffer(stagingBuffer);


	// Add & return index
	_meshes.emplace_back(vertexBuffer, indexBuffer, static_cast<uint32_t>(indices.size()));
	return _meshes.size() - 1;
}
