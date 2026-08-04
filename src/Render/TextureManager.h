#pragma once
#include "VulkanContext.h"
#include "RenderCommons.h"
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <string>
#include <unordered_map>


class VulkanRenderer;
class VulkanContext;

struct Texture
{
	AllocatedImage texImage;
	vk::raii::ImageView texImageView;
	uint32_t samplerIndex;
};

enum class TextureColorSpace
{
	sRGB,	//albedo, emissive
	HDR,
	Linear  //normal, ao, spec, etc.
};

enum class SamplerType : uint32_t
{
	RepeatAniso = 0,
	ClampEdge,
	RepeatUClampV,
	RepeatNearest,
	Count
};

static_assert(static_cast<uint32_t>(SamplerType::Count) <= MAX_TEXTURE_SAMPLERS);

class TextureManager
{
public:
	TextureManager();
	~TextureManager();

	void Initialize(VulkanRenderer* renderer, VulkanContext* vkCtx);

	TextureHandle ImportTexture(const std::string& texPath, const std::string& name, 
		TextureColorSpace colorSpace = TextureColorSpace::sRGB, SamplerType samplerType = SamplerType::RepeatAniso);
	TextureHandle ImportCubemapTexture(const std::array<std::string, 6>& texPaths, const std::string& name, TextureColorSpace colorSpace = TextureColorSpace::sRGB);
	TextureHandle CreateDefaultTexture(uint32_t colorValue, TextureColorSpace colorSpace, const std::string& name);
	TextureHandle CreateCubemapRenderTarget(const std::string& name, uint32_t resolution);
	TextureHandle CreateCubemapRenderTargetWithMips(const std::string& name, uint32_t resolution, uint32_t mipLevels);
	TextureHandle Create2DRenderTarget(const std::string& name, uint32_t width, uint32_t height);

	[[nodiscard]] const Texture& GetTexture(uint32_t index) const { return _textures[index]; }
	[[nodiscard]] vk::ImageView GetTextureImageView(uint32_t index) const { return *_textures[index].texImageView; }
	[[nodiscard]] vk::Sampler GetTextureSampler(uint32_t index) const { return *_samplers[index]; }
	[[nodiscard]] vk::Sampler GetTextureSamplerViaType(SamplerType type) const { return *_samplers[static_cast<uint32_t>(type)]; }


	[[nodiscard]] TextureHandle GetHandle(const std::string& name) const;
	[[nodiscard]] TextureHandle GetSkyboxHandle() const { return _skyboxTexHandle; };

	[[nodiscard]] size_t GetTexturesCount() const { return _textures.size(); }
	[[nodiscard]] size_t GetTextureSamplersCount() const { return _samplers.size(); }

private:
	void CreateTextureSamplers();

	TextureHandle RegisterName(const std::string& name);

	VulkanRenderer* _pRenderer = nullptr;
	VulkanContext* _pVkCtxRef = nullptr;

	std::vector<Texture> _textures;
	std::vector<vk::raii::Sampler> _samplers;

	TextureHandle _skyboxTexHandle;

	// string name -> index/handle
	std::unordered_map<std::string, uint32_t> _nameMap;
};

