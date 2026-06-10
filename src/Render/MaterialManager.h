#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include "RenderCommons.h"


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
	uint32_t samplerIndices[4];
	glm::vec4 baseColor;
	float shininess = 32.0f;
};

struct MaterialGPU
{
	uint32_t texIndices[4];
	uint32_t samplerIndices[4];
	glm::vec4 baseColor;
	float shininess;
};


class MaterialManager
{
public:
	MaterialManager();
	~MaterialManager();

	const Material& GetMaterial(MaterialHandle handle) const { return _materials[handle.index]; }

	MaterialHandle CreateUnlitMaterial(const std::string& name, uint32_t pipelineIndex, glm::vec4 color);

	MaterialHandle CreateBlinnPhongMaterial(const std::string& name, uint32_t pipelineIndex, 
		std::vector<TextureHandle> texIndices, glm::vec4 color, float shininess = 32.0f);

	std::vector<MaterialGPU> ConvertMaterialsToGPU();

	[[nodiscard]] MaterialHandle GetHandle(const std::string& name) const;

private:
	MaterialHandle RegisterName(const std::string& name);

	std::vector<Material> _materials;

	// string name -> index/handle
	std::unordered_map<std::string, uint32_t> _nameMap;
};

