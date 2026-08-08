#include "MeshManager.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "VulkanRenderer.h"
#include <filesystem>
#include <tuple>


MeshManager::MeshManager()
	: _pRenderer(nullptr)
{
	_models.reserve(MAX_OBJECTS);
}

MeshManager::~MeshManager()
{
	for (auto& model : _models)
	{
		_pRenderer->DestroyBuffer(GetBuffer(model.vbHandle));
		_pRenderer->DestroyBuffer(GetBuffer(model.ibHandle));
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
	CreatePlane();
	CreatePyramidMesh();
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

	Model model{};
	model.vbHandle = AddBufferToPool(vb);
	model.ibHandle = AddBufferToPool(ib);
	Primitive prim{
		.indexCount = static_cast<uint32_t>(idx.size())
	};
	model.primitives.push_back(prim);

	_models.push_back(model);
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
		DebugMesh{ vb, ib, glm::vec3(), glm::vec3(), static_cast<uint32_t>(aabbIndices.size()) };
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
		= DebugMesh{ vb, ib, glm::vec3(), glm::vec3(), static_cast<uint32_t>(indices.size()) };
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
		= DebugMesh{ vb, ib, glm::vec3(), glm::vec3(), static_cast<uint32_t>(indices.size()) };
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

	Model model{};
	model.vbHandle = AddBufferToPool(vb);
	model.ibHandle = AddBufferToPool(ib);
	Primitive prim{
		.indexCount = static_cast<uint32_t>(indices.size())
	};
	model.primitives.push_back(prim);

	_models.push_back(model);
	RegisterName("plane");
}

ModelHandle MeshManager::ImportModelOBJ(const std::string& meshPath, const std::string& name)
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


	Model model{};
	model.vbHandle = AddBufferToPool(vb);
	model.ibHandle = AddBufferToPool(ib);
	Primitive prim{
		.indexCount = static_cast<uint32_t>(indices.size()),
		.maxAABB = maxAABB,
		.minAABB = minAABB
	};
	model.primitives.push_back(prim);

	_models.push_back(model);
	return RegisterName(name);
}

ModelHandle MeshManager::ImportModelGLTF(const std::string& meshPath, const std::string& name)
{
	std::filesystem::path path{ meshPath };

	fastgltf::Parser parser;

	auto data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None)
		throw std::runtime_error("Can't load gltf mesh");

	auto asset = parser.loadGltfJson(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers);
	if (auto error = asset.error(); error != fastgltf::Error::None)
		throw std::runtime_error("Error occured reading buffer, parsing JSON, or validating data");

	fastgltf::Asset& gltf = asset.get();

	if (gltf.nodes.size() != 1)
	{
		//TODO: Extend to cater assets with multiple nodes
		throw std::runtime_error("Importer assumes single node asset; asset has multiple");
	}

	std::vector<Vertex>    vertices;
	std::vector<uint32_t>  indices;
	
	Model model{};

	model.intrinsicTransform = GetNodeTransform(gltf.nodes[0]);
	std::vector<MaterialHandle> matHandles = LoadGLTFMaterials(gltf, path.parent_path(), name);

	for (const fastgltf::Mesh& mesh : gltf.meshes)
	{
		model.primitives.reserve(mesh.primitives.size());
		for (const fastgltf::Primitive& prim : mesh.primitives)
		{
			Primitive primitive{};
			const size_t vertexStart = vertices.size();
			primitive.vertexOffset = vertices.size();

			// indices
			if (prim.indicesAccessor.has_value())
			{
				const fastgltf::Accessor& idxAccessor = gltf.accessors[prim.indicesAccessor.value()];

				primitive.firstIndex = indices.size();
				primitive.indexCount = idxAccessor.count;
				indices.reserve(indices.size() + idxAccessor.count);
				
				fastgltf::iterateAccessor<uint32_t>(gltf, idxAccessor,
					[&](uint32_t i) {
						indices.push_back(i);
					});
			}

			// positions
			const fastgltf::Attribute* posIt = prim.findAttribute("POSITION");
			const fastgltf::Accessor& posAccessor = gltf.accessors[posIt->accessorIndex];
			vertices.resize(vertices.size() + posAccessor.count);

			glm::vec3 minAABB{};
			glm::vec3 maxAABB{};
			bool shouldManualRecordMinMax = !posAccessor.min.has_value();

			if (shouldManualRecordMinMax)
			{
				minAABB = glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
				maxAABB = glm::vec3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
			}
			else // Acquire min/max data directly from asset
			{
				minAABB = glm::vec3(
					posAccessor.min.value().get<double>(0),
					posAccessor.min.value().get<double>(1),
					posAccessor.min.value().get<double>(2));
				maxAABB = glm::vec3(
					posAccessor.max.value().get<double>(0),
					posAccessor.max.value().get<double>(1),
					posAccessor.max.value().get<double>(2));
			}

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, posAccessor,
				[&](fastgltf::math::fvec3 p, size_t i) {
					glm::vec3 pos = { p.x(), p.y(), p.z() };
					vertices[vertexStart + i].pos = pos;

					if (shouldManualRecordMinMax)
					{
						minAABB = glm::min(minAABB, pos);
						maxAABB = glm::max(maxAABB, pos);
					}
				});

			primitive.minAABB = minAABB;
			primitive.maxAABB = maxAABB;

			// normals
			if (const auto* nrm = prim.findAttribute("NORMAL"); nrm != prim.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
					gltf, gltf.accessors[nrm->accessorIndex],
					[&](fastgltf::math::fvec3 n, size_t i) {
						vertices[vertexStart + i].normal = { n.x(), n.y(), n.z() };
					});
			}

			// texcoord 0
			if (const auto* uv = prim.findAttribute("TEXCOORD_0"); uv != prim.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
					gltf, gltf.accessors[uv->accessorIndex],
					[&](fastgltf::math::fvec2 t, size_t i) {
						vertices[vertexStart + i].texCoord = { t.x(), t.y() };
					});
			}

			// tangents (vec4: xyz = tangent, w = handedness)
			if (const auto* tan = prim.findAttribute("TANGENT"); tan != prim.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
					gltf, gltf.accessors[tan->accessorIndex],
					[&](fastgltf::math::fvec4 t, size_t i) {
						vertices[vertexStart + i].tangent = { t.x(), t.y(), t.z(), t.w() };
					});
			}

			if (prim.materialIndex.has_value())
				primitive.matHandle = matHandles[prim.materialIndex.value()];

			model.primitives.push_back(primitive);
		}
	}

	model.vbHandle = AddBufferToPool(_pRenderer->CreateDeviceLocalBuffer(vertices.data(),
		sizeof(Vertex) * vertices.size(), vk::BufferUsageFlagBits::eVertexBuffer));
	model.ibHandle = AddBufferToPool(_pRenderer->CreateDeviceLocalBuffer(indices.data(),
		sizeof(uint32_t) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer));

	_models.push_back(model);
	return RegisterName(name);
}

ModelHandle MeshManager::GetHandle(const std::string& name) const
{
	auto it = _nameMap.find(name);
	if (it == _nameMap.end())
		throw std::runtime_error("Unknown model name: " + name);
	return ModelHandle{ it->second };
}

ModelHandle MeshManager::RegisterName(const std::string& name)
{
	uint32_t idx = static_cast<uint32_t>(_models.size() - 1);
	auto [it, inserted] = _nameMap.emplace(name, idx);
	if (!inserted)
		throw std::runtime_error("Duplicate model name: " + name);
	return ModelHandle{ idx };
}

uint32_t MeshManager::AddBufferToPool(AllocatedBuffer buffer)
{
	_bufferPool.push_back(buffer);

	return _bufferPool.size() - 1;
}

glm::mat4 MeshManager::GetNodeTransform(const fastgltf::Node& node)
{
	return std::visit(fastgltf::visitor{
		[](const fastgltf::math::fmat4x4& m) 
		{
			// both column major, straight copy
			glm::mat4 out;
			std::memcpy(&out, m.data(), sizeof(float) * 16);
			return out;
		},
		[](const fastgltf::TRS& trs) 
		{
			glm::vec3 t(trs.translation.x(), trs.translation.y(), trs.translation.z());
			// glTF quat is xyzw, glm::quat ctor is wxyz
			glm::quat r(trs.rotation.w(), trs.rotation.x(), trs.rotation.y(), trs.rotation.z());
			glm::vec3 s(trs.scale.x(), trs.scale.y(), trs.scale.z());

			return glm::translate(glm::mat4(1.0f), t)
				 * glm::mat4_cast(r)
				 * glm::scale(glm::mat4(1.0f), s);
		}
		}, node.transform);
}

std::vector<MaterialHandle> MeshManager::LoadGLTFMaterials(const fastgltf::Asset& gltf, const std::filesystem::path& baseDir, const std::string& prefix)
{
	auto* texMgr = _pRenderer->GetTextureManager();
	auto* matMgr = _pRenderer->GetMaterialManager();

	std::map<std::pair<size_t, TextureColorSpace>, TextureHandle> cache;

	auto load = [&](const auto& texInfoOpt, TextureColorSpace cs) -> std::optional<TextureHandle> 
		{
			if (!texInfoOpt) 
				return std::nullopt;
			size_t img = gltf.textures[texInfoOpt->textureIndex].imageIndex.value();
			auto key = std::make_pair(img, cs);

			if (auto it = cache.find(key); it != cache.end()) 
				return it->second;
			// resolve gltf.images[img].data (sources::URI here) -> path relative to baseDir
			std::string uri{ std::get<fastgltf::sources::URI>(gltf.images[img].data).uri.string() };
			TextureHandle h = texMgr->ImportTexture((baseDir / uri).string(),
				prefix + "_img" + std::to_string(img) + (cs == TextureColorSpace::sRGB ? "_s" : "_l"), cs);
			cache.emplace(key, h);
			return h;
		};

	std::vector<MaterialHandle> out;
	for (size_t m = 0; m < gltf.materials.size(); ++m) 
	{
		const auto& gm = gltf.materials[m];
		PBRTextureSet set;
		set.baseColor = load(gm.pbrData.baseColorTexture, TextureColorSpace::sRGB);
		set.metalRough = load(gm.pbrData.metallicRoughnessTexture, TextureColorSpace::Linear);
		set.normal = load(gm.normalTexture, TextureColorSpace::Linear);
		set.emissive = load(gm.emissiveTexture, TextureColorSpace::sRGB);
		set.occlusion = load(gm.occlusionTexture, TextureColorSpace::Linear);

		glm::vec3 orm = { gm.occlusionTexture ? gm.occlusionTexture->strength : 1.0f,
						  gm.pbrData.roughnessFactor, gm.pbrData.metallicFactor };
		out.push_back(matMgr->CreatePBRMaterial(prefix + "_mat" + std::to_string(m),
			_pRenderer->GetPBRPipelineIndex(), set,
			glm::make_vec4(gm.pbrData.baseColorFactor.data()), orm,
			glm::make_vec3(gm.emissiveFactor.data()),
			gm.normalTexture ? gm.normalTexture->scale : 1.0f, gm.alphaCutoff));
	}
	return out;
}
