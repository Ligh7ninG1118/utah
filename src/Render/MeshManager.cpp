#include "MeshManager.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "VulkanRenderer.h"

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

	for (auto& mesh : _debugMeshes)
	{
		_pRenderer->DestroyBuffer(mesh.vertexBuffer);
		_pRenderer->DestroyBuffer(mesh.indexBuffer);
	}
}

void MeshManager::Initialize(VulkanRenderer* renderer)
{
	_pRenderer = renderer;

	CreateUnitCubeMesh();
	CreateAABBMesh();
	CreateIcosphereMesh(1);
	CreatePyramidMesh();
	CreatePlane();
}

void MeshManager::CreateUnitCubeMesh()
{
	std::vector<glm::vec3> pos = {
	{-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
	{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1} };
	std::vector<uint32_t> idx = {
		0,1,2, 2,3,0,  4,5,6, 6,7,4,  0,4,7, 7,3,0,
		1,5,6, 6,2,1,  3,2,6, 6,7,3,  0,1,5, 5,4,0 };

	AllocatedBuffer vb = _pRenderer->CreateDeviceLocalBuffer(pos.data(),
		sizeof(glm::vec3) * pos.size(), vk::BufferUsageFlagBits::eVertexBuffer);

	AllocatedBuffer ib = _pRenderer->CreateDeviceLocalBuffer(idx.data(),
		sizeof(uint32_t) * idx.size(), vk::BufferUsageFlagBits::eIndexBuffer);

	_meshes.emplace_back(vb, ib, glm::vec3(), glm::vec3(), static_cast<uint32_t>(idx.size()));
	RegisterName("unit_cube");
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

	AllocatedBuffer vb = _pRenderer->CreateDeviceLocalBuffer(aabbVertices.data(),
		sizeof(glm::vec3) * aabbVertices.size(), vk::BufferUsageFlagBits::eVertexBuffer);

	AllocatedBuffer ib = _pRenderer->CreateDeviceLocalBuffer(aabbIndices.data(),
		sizeof(uint32_t) * aabbIndices.size(), vk::BufferUsageFlagBits::eIndexBuffer);

	_debugMeshes[static_cast<uint32_t>(DebugMeshType::AABB)] = 
		Mesh{ vb, ib, glm::vec3(), glm::vec3(), static_cast<uint32_t>(aabbIndices.size()) };
}

void MeshManager::CreatePyramidMesh(float baseSize, float height)
{
	const float h = baseSize * 0.5f;

	std::vector<glm::vec3> positions = {
		{ -h, 0.0f, -h }, // 0 bl
		{  h, 0.0f, -h }, // 1 br
		{  h, 0.0f,  h }, // 2 fr
		{ -h, 0.0f,  h }, // 3 fl
		{ 0.0f, height, 0.0f }, // 4 apex
	};;
	std::vector<uint32_t> indices = {
		0, 1,  1, 2,  2, 3,  3, 0,  // base square
		0, 4,  1, 4,  2, 4,  3, 4,  // spokes to apex
	};

	AllocatedBuffer vb = _pRenderer->CreateDeviceLocalBuffer(positions.data(),
		sizeof(glm::vec3) * positions.size(), vk::BufferUsageFlagBits::eVertexBuffer);

	AllocatedBuffer ib = _pRenderer->CreateDeviceLocalBuffer(indices.data(),
		sizeof(uint32_t) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer);

	_debugMeshes[static_cast<uint32_t>(DebugMeshType::Pyramid)] 
		= Mesh{ vb, ib, glm::vec3(), glm::vec3(), static_cast<uint32_t>(indices.size()) };
}

void MeshManager::CreateIcosphereMesh(uint32_t subdivisions, float radius)
{
	const float t = (1.0f + std::sqrt(5.0f)) * 0.5f; // golden ratio

	std::vector<glm::vec3> positions = {
		{-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
		{ 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
		{ t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1},
	};
	for (auto& p : positions)
		p = glm::normalize(p);

	std::vector<uint32_t> indices = {
		0,11,5,  0,5,1,   0,1,7,   0,7,10,  0,10,11,
		1,5,9,   5,11,4,  11,10,2, 10,7,6,  7,1,8,
		3,9,4,   3,4,2,   3,2,6,   3,6,8,   3,8,9,
		4,9,5,   2,4,11,  6,2,10,  8,6,7,   9,8,1
	};

	// midpoint cache keyed by ordered edge (a,b)
	std::unordered_map<uint64_t, uint32_t> midCache;
	auto midpoint = [&](uint32_t a, uint32_t b) -> uint32_t
		{
			uint64_t key = a < b ? (uint64_t(a) << 32 | b) : (uint64_t(b) << 32 | a);
			auto it = midCache.find(key);
			if (it != midCache.end()) return it->second;

			glm::vec3 m = glm::normalize((positions[a] + positions[b]) * 0.5f);
			uint32_t idx = uint32_t(positions.size());
			positions.push_back(m);
			midCache.emplace(key, idx);
			return idx;
		};

	for (uint32_t s = 0; s < subdivisions; ++s) {
		std::vector<uint32_t> next;
		next.reserve(indices.size() * 4);
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			uint32_t v0 = indices[i], v1 = indices[i + 1], v2 = indices[i + 2];
			uint32_t a = midpoint(v0, v1);
			uint32_t b = midpoint(v1, v2);
			uint32_t c = midpoint(v2, v0);
			next.insert(next.end(), { v0,a,c,  v1,b,a,  v2,c,b,  a,b,c });
		}
		indices = std::move(next);
	}

	if (radius != 1.0f)
		for (auto& p : positions)
			p *= radius;

	AllocatedBuffer vb = _pRenderer->CreateDeviceLocalBuffer(positions.data(),
		sizeof(glm::vec3) * positions.size(), vk::BufferUsageFlagBits::eVertexBuffer);

	AllocatedBuffer ib = _pRenderer->CreateDeviceLocalBuffer(indices.data(),
		sizeof(uint32_t) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer);


	_debugMeshes[static_cast<uint32_t>(DebugMeshType::Icosphere)] 
		= Mesh{ vb, ib, glm::vec3(), glm::vec3(), static_cast<uint32_t>(indices.size()) };
}

void MeshManager::CreatePlane()
{
	std::vector<Vertex> vertices = {
		// pos                    normal               texCoord
		{{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, // 0
		{{ 0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // 1
		{{ 0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // 2
		{{-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // 3
	};

	std::vector<uint32_t> indices = {
		0, 3, 2,
		0, 2, 1,
	};

	AllocatedBuffer vb = _pRenderer->CreateDeviceLocalBuffer(vertices.data(),
		sizeof(Vertex) * vertices.size(), vk::BufferUsageFlagBits::eVertexBuffer);

	AllocatedBuffer ib = _pRenderer->CreateDeviceLocalBuffer(indices.data(),
		sizeof(uint32_t) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer);


	_meshes.emplace_back(vb, ib, glm::vec3(), glm::vec3(), static_cast<uint32_t>(indices.size()));
	RegisterName("plane");
}

MeshHandle MeshManager::ImportMesh(const std::string& meshPath, const std::string& name)
{
	tinyobj::attrib_t                attrib;
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      err;
	std::string                      warn;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, meshPath.c_str()))
	{
		throw std::runtime_error(err);
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

	AllocatedBuffer vb = _pRenderer->CreateDeviceLocalBuffer(vertices.data(),
		sizeof(Vertex) * vertices.size(), vk::BufferUsageFlagBits::eVertexBuffer);

	AllocatedBuffer ib = _pRenderer->CreateDeviceLocalBuffer(indices.data(),
		sizeof(uint32_t) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer);

	// Add & return handle
	_meshes.emplace_back(vb, ib, maxAABB, minAABB, static_cast<uint32_t>(indices.size()));
	
	return RegisterName(name);
}

MeshHandle MeshManager::GetHandle(const std::string& name) const
{
	auto it = _nameMap.find(name);
	if (it == _nameMap.end())
		throw std::runtime_error("Unknown mesh name: " + name);
	return MeshHandle{ it->second };
}

MeshHandle MeshManager::RegisterName(const std::string& name)
{
	uint32_t idx = static_cast<uint32_t>(_meshes.size() - 1);
	auto [it, inserted] = _nameMap.emplace(name, idx);
	if (!inserted)
		throw std::runtime_error("Duplicate mesh name: " + name);
	return MeshHandle{ idx };
}
