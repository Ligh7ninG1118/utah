#pragma once
#include "VulkanContext.h"
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <string>

class VulkanRenderer;
class VulkanContext;

struct Texture
{
	AllocatedImage texImage;
	vk::raii::ImageView texImageView;
	uint32_t samplerIndex;
};


class TextureManager
{
public:
	TextureManager();
	~TextureManager();

	void Initialize(VulkanRenderer* renderer, VulkanContext* vkCtx);

	uint32_t ImportTexture(const std::string& texPath);

	vk::ImageView GetTextureImageView(uint32_t index) const { return *_textures[index].texImageView; }
	vk::Sampler GetTextureSampler(uint32_t index) const { return *_samplers[_textures[index].samplerIndex]; }

	uint32_t GetTexturesCount() const { return _textures.size(); }

private:
	void CreateTextureSampler();

	VulkanRenderer* _pRenderer = nullptr;
	VulkanContext* _pVkCtxRef = nullptr;

	std::vector<Texture> _textures;
	std::vector<vk::raii::Sampler> _samplers;
};

