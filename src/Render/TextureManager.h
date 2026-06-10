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
	Linear  //normal, ao, spec, etc.
};

class TextureManager
{
public:
	TextureManager();
	~TextureManager();

	void Initialize(VulkanRenderer* renderer, VulkanContext* vkCtx);

	TextureHandle ImportTexture(const std::string& texPath, const std::string& name, TextureColorSpace colorSpace = TextureColorSpace::sRGB);
	TextureHandle CreateWhiteTexture();

	[[nodiscard]] vk::ImageView GetTextureImageView(uint32_t index) const { return *_textures[index].texImageView; }
	[[nodiscard]] vk::Sampler GetTextureSampler(uint32_t index) const { return *_samplers[index]; }

	[[nodiscard]] TextureHandle GetHandle(const std::string& name) const;

	[[nodiscard]] size_t GetTexturesCount() const { return _textures.size(); }
	[[nodiscard]] size_t GetTextureSamplersCount() const { return _samplers.size(); }

private:
	void CreateTextureSampler();

	TextureHandle RegisterName(const std::string& name);

	VulkanRenderer* _pRenderer = nullptr;
	VulkanContext* _pVkCtxRef = nullptr;

	std::vector<Texture> _textures;
	std::vector<vk::raii::Sampler> _samplers;

	// string name -> index/handle
	std::unordered_map<std::string, uint32_t> _nameMap;
};

