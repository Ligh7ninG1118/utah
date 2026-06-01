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
	float shininess = 32.0f;
};

struct MaterialGPU
{
	uint32_t texIndices[4];
	glm::vec4 baseColor;
	float shininess;
};


class MaterialManager
{
public:
	MaterialManager();
	~MaterialManager();

	const Material& GetMaterial(uint32_t index) const { return _materials[index]; }

	uint32_t CreateUnlitMaterial(uint32_t pipelineIndex, glm::vec4 color);

	uint32_t CreateBlinnPhongMaterial(uint32_t pipelineIndex, std::vector<uint32_t> texIndices, glm::vec4 color, float shininess = 32.0f);

	std::vector<MaterialGPU> ConvertMaterialsToGPU();

private:
	std::vector<Material> _materials;
};

