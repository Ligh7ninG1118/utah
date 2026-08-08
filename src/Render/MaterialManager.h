#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <optional>
#include "RenderCommons.h"


enum class MaterialType : uint32_t
{
	Unlit,
	//BlinnPhong, // deprecated
	PBR
};

enum class PBRSlot : uint32_t
{
	BaseColor = 0,
	MetalRough = 1,
	Normal = 2,
	Emissive = 3,
	Occlusion = 4,

	Count
};

inline constexpr uint32_t ToIdx(PBRSlot s) { return static_cast<uint32_t>(s); }

static constexpr uint32_t MAX_TEX_SLOTS = 8;

struct PBRTextureSet
{
	std::optional<TextureHandle> baseColor;
	std::optional<TextureHandle> metalRough;
	std::optional<TextureHandle> normal;
	std::optional<TextureHandle> emissive;
	std::optional<TextureHandle> occlusion;
};

struct Material
{
	MaterialType type;
	uint32_t pipeline;
	uint32_t texIndices[MAX_TEX_SLOTS];
	uint32_t samplerIndices[MAX_TEX_SLOTS];
	glm::vec4 baseColorFactor;
	glm::vec4 ormFactor; // r = ao strength, g = roughness, b = metallic
	glm::vec4 emissiveFactor;
	glm::vec4 params;  // r = normal scale, g = alpha cutoff, b/a = reserved
};

struct MaterialGPU
{
	uint32_t texIndices[MAX_TEX_SLOTS];
	uint32_t samplerIndices[MAX_TEX_SLOTS];
	glm::vec4 baseColorFactor;
	glm::vec4 ormFactor;
	glm::vec4 emissiveFactor;
	glm::vec4 params;
};


class MaterialManager
{
public:
	MaterialManager();
	~MaterialManager();

	const Material& GetMaterial(MaterialHandle handle) const { return _materials[handle.index]; }

	MaterialHandle CreateUnlitMaterial(const std::string& name, uint32_t pipelineIndex, glm::vec4 color);

	MaterialHandle CreatePBRMaterial(const std::string& name, uint32_t pipelineIndex, 
		const PBRTextureSet& textures,
		glm::vec4 baseColorFactor = glm::vec4(1.0f), 
		glm::vec3 ormFactor = glm::vec3(1.0f),
		glm::vec3 emissiveFactor = glm::vec3(0.0f), 
		float normalScale = 1.0f,
		float alphaCutoff = 0.5f);

	std::vector<MaterialGPU> ConvertMaterialsToGPU();

	[[nodiscard]] MaterialHandle GetHandle(const std::string& name) const;

private:
	MaterialHandle RegisterName(const std::string& name);

	std::vector<Material> _materials;

	// string name -> index/handle
	std::unordered_map<std::string, uint32_t> _nameMap;
};

