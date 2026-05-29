#pragma once
#include "Core/UtahCtx.h"


#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
// #define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "VulkanContext.h"
#include "RenderComponent.h"
#include "TextureManager.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "Gameplay/CameraComponent.h"
#include "Render/GPUTypes.h"

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texCoord;

	static vk::VertexInputBindingDescription GetBindingDescription()
	{
		return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
		// Input rate can be changed for instanced drawing
	}

	static std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		return {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
			vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)),
		};
	}

	bool operator==(const Vertex& other) const
	{
		return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
	}
};

template <>
struct std::hash<Vertex>
{
	size_t operator()(const Vertex& vertex) const
	{
		return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
			(hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};


class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	void Initialize();

	void UpdateDrawList(std::vector<struct DrawJob>&& list);

	void UpdateCamera(const CameraComponent& camera);

	void UpdateLights(std::vector<PointLightGPU>&& lights);

	void DrawFrame();

	void WaitForIdle();

	void NotifyResized() { _framebufferResized = true; }

	//TODO: Sort these into a separate ResourceFactory class

	[[nodiscard]] AllocatedImage CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples,
		vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO);
	[[nodiscard]] vk::raii::ImageView CreateImageView(vk::Image image, vk::Format format,
		vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) const;

	void TransitionImageLayout(vk::Image image, vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout, uint32_t mipLevels);
	void TransitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
		vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
		vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
		vk::ImageAspectFlags aspectMask);

	void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);
	void GenerateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	// Buffer Creation and Data Transfer
	[[nodiscard]] AllocatedBuffer CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
		VmaMemoryUsage memUsage, VmaAllocationCreateFlags allocFlags = 0);
	void DestroyBuffer(AllocatedBuffer& buffer);
	void DestroyImage(AllocatedImage& image);
	void CreateUniformBuffers();
	void CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

	MeshManager* GetMeshManager() { return &_meshManager; }

private:
	// ImGUI stuff
	void InitImGUI();


	// Per-Frame Update
	void UpdateUniformBuffer(uint32_t currentImage);

	// Window resize callback
	void RegisterResizeCallback();
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

	// Swap Chain
	struct SwapChainSupportDetails
	{
		vk::SurfaceCapabilitiesKHR        capabilities;
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR>   presentModes;
	};

	// Resize Callback
	static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
	static vk::PresentModeKHR   ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
	static uint32_t             ChooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities);
	[[nodiscard]] vk::Extent2D  ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;

	void CreateSwapChain();
	void CreateImageViews();
	// Swap Chain Recreation
	void CleanupSwapChain();
	void RecreateSwapChain();

	// Descriptor Creation
	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void CreateDescriptorSets();

	// Graphics Pipeline
	uint32_t CreateGraphicsPipeline(const std::string& vertPath, const std::string& fragPath);

	// Command Pool & Buffers
	void CreateCommandPool();
	void CreateCommandBuffers();
	void RecordCommandBuffer(uint32_t imageIndex);
	std::unique_ptr<vk::raii::CommandBuffer> BeginSingleTimeCommands();
	void EndSingleTimeCommands(const vk::raii::CommandBuffer& commandBuffer);

	// Depth Buffer
	void CreateDepthResources();
	vk::Format FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
		vk::FormatFeatureFlags features) const;
	vk::Format FindDepthFormat() const;
	bool HasStencilComponent(vk::Format format) const;

	// Synchronization
	void CreateSyncObjects();

	// Shader Loading
	std::vector<char> ReadFile(const std::string& filename);
	[[nodiscard]] vk::raii::ShaderModule CreateShaderModule(const std::vector<char>& code) const;

	// Multiple Sampling
	vk::SampleCountFlagBits GetMaxUsableSampleCount() const;
	void CreateColorResources();

	// Variables
	// Ref to windows and program context
	UtahCtx& _programCtx;

	// Handles instance creation, device management, etc.
	VulkanContext _vkCtx;
	
	vk::raii::SwapchainKHR           _swapChain = nullptr;
	std::vector<vk::Image>           _swapChainImages;
	vk::SurfaceFormatKHR             _swapChainSurfaceFormat;
	vk::Extent2D                     _swapChainExtent;
	std::vector<vk::raii::ImageView> _swapChainImageViews;

	vk::raii::DescriptorSetLayout _globalDescriptorSetLayout = nullptr;
	vk::raii::PipelineLayout      _pipelineLayout = nullptr;
	std::vector<vk::raii::Pipeline> _pipelines;
	//vk::raii::Pipeline            _graphicsPipeline = nullptr;

	AllocatedImage		   _colorImage{};
	vk::raii::ImageView    _colorImageView = nullptr;

	AllocatedImage		   _depthImage{};
	vk::raii::ImageView    _depthImageView = nullptr;

	TextureManager _textureManger;
	MeshManager _meshManager;
	MaterialManager _materialManager;

	std::vector<AllocatedBuffer> _cameraUBOs;
	std::vector<AllocatedBuffer> _lightUBOs;
	std::vector<AllocatedBuffer> _objectSSBOs;
	std::vector<AllocatedBuffer> _materialSSBOs;


	vk::raii::DescriptorPool             _descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> _globalDescriptorSets;

	vk::raii::DescriptorPool			 _imguiDescriptorPool = nullptr;

	vk::raii::CommandPool                _commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> _commandBuffers;

	std::vector<vk::raii::Semaphore> _presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;
	std::vector<vk::raii::Fence>     _inFlightFences;

	uint32_t _semaphoreIndex = 0;
	uint32_t _currentFrame = 0;

	bool _framebufferResized = false;

	vk::SampleCountFlagBits _msaaSamples = vk::SampleCountFlagBits::e1;

	std::vector<struct DrawJob> _drawList;

	struct CameraComponent _mainCam;

	std::vector<PointLightGPU> _pointLights;
};

