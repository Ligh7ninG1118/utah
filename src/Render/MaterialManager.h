#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>


enum class MaterialType : uint32_t
{
	Unlit,
	BlinnPhong,
	PBR
};

struct Material
{
	MaterialType type;
	uint32_t pipeline;
	uint32_t texIndices[4];
	glm::vec4 baseColor;
};


class MaterialManager
{
public:
	MaterialManager();
	~MaterialManager();

	uint32_t ImportMesh();

private:
	std::vector<Material> _materials;

};

