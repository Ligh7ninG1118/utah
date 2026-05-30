#include "MeshManager.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>

#include "VulkanRenderer.h"
#include "RenderCommons.h"

MeshManager::MeshManager()
{
	_meshes.reserve(MAX_OBJECTS);
}

MeshManager::~MeshManager()
{
	for (auto& mesh : _meshes)
	{
		_pRenderer->DestroyBuffer(mesh.vertexBuffer);
		_pRenderer->DestroyBuffer(mesh.indexBuffer);
	}

	_pRenderer->DestroyBuffer(_aabbMesh.vertexBuffer);
	_pRenderer->DestroyBuffer(_aabbMesh.indexBuffer);
}

void MeshManager::Initialize(VulkanRenderer* renderer)
{
	_pRenderer = renderer;

	CreateAABBMesh();
}

void MeshManager::CreateAABBMesh()
{
	std::vector<glm::vec3> aabbVertices = {
				{-0.5f, -0.5f, -0.5f}, // 0
				{ 0.5f, -0.5f, -0.5f}, // 1
				{ 0.5f,  0.5f, -0.5f}, // 2
				{-0.5f,  0.5f, -0.5f}, // 3
				{-0.5f, -0.5f,  0.5f}, // 4
				{ 0.5f, -0.5f,  0.5f}, // 5
				{ 0.5f,  0.5f,  0.5f}, // 6
				{-0.5f,  0.5f,  0.5f}, // 7
	};

	std::vector<uint32_t> aabbIndices = {
		// bottom face (z = -0.5)
		0,1, 1,2, 2,3, 3,0,
		// top face (z = +0.5)
		4,5, 5,6, 6,7, 7,4,
		// vertical edges connecting bottom to top
		0,4, 1,5, 2,6, 3,7,
	};

	//TODO: Replace with a function, for both this and ImportMesh function

	// Create Vertex Buffer
	vk::DeviceSize bufferSize = sizeof(aabbVertices[0]) * aabbVertices.size();

	AllocatedBuffer stagingBuffer = _pRenderer->CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

	memcpy(stagingBuffer.info.pMappedData, aabbVertices.data(), bufferSize);

	AllocatedBuffer vertexBuffer = _pRenderer->CreateBuffer(bufferSize,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		VMA_MEMORY_USAGE_AUTO);

	_pRenderer->CopyBuffer(stagingBuffer.buffer, vertexBuffer.buffer, bufferSize);

	_pRenderer->DestroyBuffer(stagingBuffer);


	// Create Index Buffer
	bufferSize = sizeof(aabbIndices[0]) * aabbIndices.size();

	stagingBuffer = _pRenderer->CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

	memcpy(stagingBuffer.info.pMappedData, aabbIndices.data(), bufferSize);

	AllocatedBuffer indexBuffer = _pRenderer->CreateBuffer(bufferSize,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		VMA_MEMORY_USAGE_AUTO);

	_pRenderer->CopyBuffer(stagingBuffer.buffer, indexBuffer.buffer, bufferSize);

	_pRenderer->DestroyBuffer(stagingBuffer);

	_aabbMesh = Mesh{ vertexBuffer, indexBuffer, glm::vec3(), glm::vec3(), static_cast<uint32_t>(aabbIndices.size())};
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

	glm::vec3 minAABB = glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	glm::vec3 maxAABB = glm::vec3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

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

			// this works per component wise
			minAABB = glm::min(vertex.pos, minAABB);
			maxAABB = glm::max(vertex.pos, maxAABB);

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
	_meshes.emplace_back(vertexBuffer, indexBuffer, maxAABB, minAABB, static_cast<uint32_t>(indices.size()));
	return _meshes.size() - 1;
}
