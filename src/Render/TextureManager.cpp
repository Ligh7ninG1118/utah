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
	CreateWhiteTexture();

	_skyboxTexHandle = ImportCubemapTexture({ "textures/skybox test x pos.png",
											"textures/skybox test x neg.png",
											"textures/skybox test y pos.png",
											"textures/skybox test y neg.png",
											"textures/skybox test z pos.png",
											"textures/skybox test z neg.png", }, "skybox_test");
}

TextureHandle TextureManager::ImportTexture(const std::string& texPath, const std::string& name, TextureColorSpace colorSpace)
{
	if (_pRenderer == nullptr)
	{
		throw std::runtime_error("Texture Manager not initialized");
	}

	int texWidth, texHeight, texChannels;

	void* pixels;
	vk::DeviceSize imageSize;

	if (colorSpace == TextureColorSpace::HDR)
	{
		pixels = static_cast<float*>(stbi_loadf(texPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha));
		imageSize = texWidth * texHeight * 16;
	}
	else
	{
		pixels = static_cast<stbi_uc*>(stbi_load(texPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha));
		imageSize = texWidth * texHeight * 4;
	}

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

	vk::Format format = vk::Format::eUndefined;

	switch (colorSpace)
	{
	case TextureColorSpace::sRGB:
		format = vk::Format::eR8G8B8A8Srgb;
		break;
	case TextureColorSpace::HDR:
		format = vk::Format::eR32G32B32A32Sfloat;
		break;
	case TextureColorSpace::Linear:
		format = vk::Format::eR8G8B8A8Unorm;
		break;
	default:
		break;
	}

	// Create Image
	AllocatedImage textureImage = _pRenderer->CreateImage(texWidth, texHeight, mipLevels,
		vk::SampleCountFlagBits::e1,
		format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);

	_pRenderer->TransitionImageLayout(textureImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
	_pRenderer->CopyBufferToImage(stagingBuffer.buffer, textureImage.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	_pRenderer->GenerateMipmaps(textureImage.image, format, texWidth, texHeight, mipLevels);

	_pRenderer->DestroyBuffer(stagingBuffer);

	// Create Image View
	vk::raii::ImageView textureImageView =
		_pRenderer->CreateImageView(textureImage.image, format, vk::ImageAspectFlagBits::eColor, mipLevels);

	// Add & return index
	_textures.emplace_back(
		textureImage, 
		std::move(textureImageView), 
		0 /*use default linear repeat sampler*/);

    return RegisterName(name);
}

// Cubemap order: pos x, neg x, pos y, neg y, pos z, neg z
TextureHandle TextureManager::ImportCubemapTexture(const std::array<std::string, 6>& texPaths, const std::string& name, TextureColorSpace colorSpace)
{
	if (texPaths.size() != 6)
	{
		throw std::runtime_error("Cubemap texture size is not 6");
	}

	std::array<stbi_uc*, 6> pixelBytes;
	int texWidth = 0, texHeight = 0, texChannels;

	for (size_t i = 0; i < texPaths.size(); i++)
	{
		int w, h;
		pixelBytes[i] = stbi_load(texPaths[i].c_str(), &w, &h, &texChannels, STBI_rgb_alpha);
		if (!pixelBytes[i])
			throw std::runtime_error("Failed to load texture image!");

		if (i == 0)
		{
			texWidth = w;
			texHeight = h;
		}
		else if (w != texWidth || h != texHeight)
		{
			throw std::runtime_error("Cubemap faces are not uniform in dimension");
		}
	}

	if (texWidth != texHeight)
		throw std::runtime_error("Cubemap faces not square");

	vk::DeviceSize imageSize = texWidth * texHeight * 4;

	AllocatedBuffer stagingBuffer = _pRenderer->CreateBuffer(
		imageSize * 6,
		vk::BufferUsageFlagBits::eTransferSrc,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);


	auto* dst = static_cast<std::byte*>(stagingBuffer.info.pMappedData);
	for (size_t i = 0; i < 6; i++)
	{
		memcpy(dst + i * imageSize, pixelBytes[i], imageSize);
		stbi_image_free(pixelBytes[i]);
	}


	vk::Format format = vk::Format::eUndefined;

	switch (colorSpace)
	{
	case TextureColorSpace::sRGB:
		format = vk::Format::eR8G8B8A8Srgb;
		break;
	case TextureColorSpace::HDR:
		format = vk::Format::eR16G16B16A16Sfloat;
		break;
	case TextureColorSpace::Linear:
		format = vk::Format::eR8G8B8A8Unorm;
		break;
	default:
		break;
	}

	uint32_t mipLevels = 1;

	// Create Image
	AllocatedImage textureImage = _pRenderer->CreateImage(texWidth, texHeight, mipLevels,
		vk::SampleCountFlagBits::e1,
		format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		VMA_MEMORY_USAGE_AUTO,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		6);

	_pRenderer->TransitionImageLayout(textureImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels, 6);
	_pRenderer->CopyBufferToImage(stagingBuffer.buffer, textureImage.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 6);
	_pRenderer->TransitionImageLayout(textureImage.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, mipLevels, 6);

	_pRenderer->DestroyBuffer(stagingBuffer);

	// Create Image View
	vk::raii::ImageView textureImageView =
		_pRenderer->CreateImageView(textureImage.image,
			format,
			vk::ImageAspectFlagBits::eColor,
			mipLevels,
			vk::ImageViewType::eCube,
			6);

	// Add & return index
	_textures.emplace_back(
		textureImage,
		std::move(textureImageView),
		0 /*use default linear repeat sampler*/);

	return RegisterName(name);
}

TextureHandle TextureManager::CreateWhiteTexture()
{
	uint32_t white = 0xFFFFFFFF;            // RGBA 255,255,255,255
	vk::DeviceSize imageSize = 4;

	AllocatedBuffer staging = _pRenderer->CreateBuffer(
		imageSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
	memcpy(staging.info.pMappedData, &white, imageSize);

	AllocatedImage img = _pRenderer->CreateImage(
		1, 1, 1, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);   // no TransferSrc — no mip blits

	_pRenderer->TransitionImageLayout(img.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1);
	_pRenderer->CopyBufferToImage(staging.buffer, img.image, 1, 1);
	_pRenderer->TransitionImageLayout(img.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 1);
	_pRenderer->DestroyBuffer(staging);

	vk::raii::ImageView view = _pRenderer->CreateImageView(img.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, 1);
	_textures.emplace_back(img, std::move(view), 0);
	return RegisterName("white");
}

TextureHandle TextureManager::CreateCubemapRenderTarget(const std::string& name, uint32_t resolution)
{
	vk::Format format = vk::Format::eR16G16B16A16Sfloat;

	uint32_t mipLevels = 1;

	// Create Image
	AllocatedImage textureImage = _pRenderer->CreateImage(resolution, resolution, mipLevels,
		vk::SampleCountFlagBits::e1,
		format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
		VMA_MEMORY_USAGE_AUTO,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		6);

	// Create Image View
	vk::raii::ImageView textureImageView =
		_pRenderer->CreateImageView(textureImage.image,
			format,
			vk::ImageAspectFlagBits::eColor,
			mipLevels,
			vk::ImageViewType::eCube,
			6);

	// Add & return index
	_textures.emplace_back(
		textureImage,
		std::move(textureImageView),
		0 /*use default linear repeat sampler*/);

	return RegisterName(name);
}

TextureHandle TextureManager::CreateCubemapRenderTargetWithMips(const std::string& name, uint32_t resolution, uint32_t mipLevels)
{
	vk::Format format = vk::Format::eR16G16B16A16Sfloat;

	AllocatedImage textureImage = _pRenderer->CreateImage(resolution, resolution, mipLevels,
		vk::SampleCountFlagBits::e1,
		format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
		VMA_MEMORY_USAGE_AUTO,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		6);

	// Full-mip view used for runtime sampling (per-mip render views are created at draw time)
	vk::raii::ImageView textureImageView =
		_pRenderer->CreateImageView(textureImage.image,
			format,
			vk::ImageAspectFlagBits::eColor,
			mipLevels,
			vk::ImageViewType::eCube,
			6);

	_textures.emplace_back(
		textureImage,
		std::move(textureImageView),
		0);

	return RegisterName(name);
}

TextureHandle TextureManager::Create2DRenderTarget(const std::string& name, uint32_t width, uint32_t height)
{
	vk::Format format = vk::Format::eR16G16B16A16Sfloat;
	uint32_t mipLevels = 1;

	AllocatedImage textureImage = _pRenderer->CreateImage(width, height, mipLevels,
		vk::SampleCountFlagBits::e1,
		format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment);

	vk::raii::ImageView textureImageView =
		_pRenderer->CreateImageView(textureImage.image, format, vk::ImageAspectFlagBits::eColor, mipLevels);

	_textures.emplace_back(
		textureImage,
		std::move(textureImageView),
		0);

	return RegisterName(name);
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
											 .compareOp = vk::CompareOp::eAlways,
											 .maxLod = vk::LodClampNone};

	_samplers.emplace_back(_pVkCtxRef->GetDevice(), samplerInfo);
}

TextureHandle TextureManager::GetHandle(const std::string& name) const
{
	auto it = _nameMap.find(name);
	if (it == _nameMap.end())
		throw std::runtime_error("Unknown texture name: " + name);
	return TextureHandle{ it->second };
}

TextureHandle TextureManager::RegisterName(const std::string& name)
{
	uint32_t idx = static_cast<uint32_t>(_textures.size() - 1);
	auto [it, inserted] = _nameMap.emplace(name, idx);
	if (!inserted)
		throw std::runtime_error("Duplicate texture name: " + name);
	return TextureHandle{ idx };
}