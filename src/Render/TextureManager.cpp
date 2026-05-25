#include "TextureManager.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "VulkanRenderer.h"
#include "RenderCommons.h"


TextureManager::TextureManager()
{
	_textures.reserve(MAX_TEXTURES);
}

TextureManager::~TextureManager()
{
	for (auto &texture : _textures)
	{
		texture.texImageView = nullptr;
		_pRenderer->DestroyImage(texture.texImage);
	}
}

void TextureManager::Initialize(VulkanRenderer* renderer, VulkanContext* vkCtx)
{
	_pRenderer = renderer;
	_pVkCtxRef = vkCtx;

	CreateTextureSampler();
}

uint32_t TextureManager::ImportTexture(const std::string& texPath)
{
	if (_pRenderer == nullptr)
	{
		throw std::runtime_error("Texture Manager not initialized");
	}

	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(texPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	vk::DeviceSize imageSize = texWidth * texHeight * 4;
	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	if (!pixels)
	{
		throw std::runtime_error("Failed to load texture image!");
	}

	AllocatedBuffer stagingBuffer = _pRenderer->CreateBuffer(
		imageSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

	memcpy(stagingBuffer.info.pMappedData, pixels, imageSize);

	stbi_image_free(pixels);

	// Create Image
	AllocatedImage textureImage = _pRenderer->CreateImage(texWidth, texHeight, mipLevels,
		vk::SampleCountFlagBits::e1,
		vk::Format::eR8G8B8A8Srgb,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);

	_pRenderer->TransitionImageLayout(textureImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
	_pRenderer->CopyBufferToImage(stagingBuffer.buffer, textureImage.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	_pRenderer->GenerateMipmaps(textureImage.image, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, mipLevels);

	_pRenderer->DestroyBuffer(stagingBuffer);

	// Create Image View
	vk::raii::ImageView textureImageView =
		_pRenderer->CreateImageView(textureImage.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels);

	// Add & return index
	_textures.emplace_back(
		textureImage, 
		std::move(textureImageView), 
		0 /*use default linear repeat sampler*/);

    return _textures.size() - 1;
}

// Only create one sampler (linear repeat) for now
void TextureManager::CreateTextureSampler()
{
	vk::PhysicalDeviceProperties properties = _pVkCtxRef->GetPhysicalDevice().getProperties();
	vk::SamplerCreateInfo        samplerInfo{ .magFilter = vk::Filter::eLinear,
											 .minFilter = vk::Filter::eLinear,
											 .mipmapMode = vk::SamplerMipmapMode::eLinear,
											 .addressModeU = vk::SamplerAddressMode::eRepeat,
											 .addressModeV = vk::SamplerAddressMode::eRepeat,
											 .addressModeW = vk::SamplerAddressMode::eRepeat,
											 .mipLodBias = 0.0f,
											 .anisotropyEnable = vk::True,
											 .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
											 .compareEnable = vk::False,
											 .compareOp = vk::CompareOp::eAlways };

	_samplers.emplace_back(_pVkCtxRef->GetDevice(), samplerInfo);
}