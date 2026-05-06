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

#include "RenderComponent.h"
#include "Gameplay/CameraComponent.h"
#include "Render/PointLightData.h"

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

struct CameraUBO
{
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

struct alignas(16) LightUBO
{
	glm::vec3 eyePos;
	unsigned int pointLightNum;

	PointLightData pointLights[32];
};

struct ObjectUBO
{
	alignas(16) glm::mat4 model;
};


class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	void Initialize(std::vector<RenderComponent>& pool);

	void UpdateDrawList(std::vector<struct DrawJob>& list);

	void UpdateCamera(const CameraComponent& camera);

	void UpdateLights(std::vector<PointLightData> lights);

	void DrawFrame();

	void WaitForIdle();

	void NotifyResized() { _framebufferResized = true; }

private:

	// Per-Frame Update
	void UpdateUniformBuffer(uint32_t currentImage);

	// Window resize callback
	void RegisterResizeCallback();
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

	// Core Vulkan Setup
	void CreateInstance();
	std::vector<const char*> GetRequiredExtensions() const;
	void SetupDebugMessenger();
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT             messageType,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	void CreateSurface();
	void PickPhysicalDevice();
	void CreateLogicalDevice();

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
	void CreateDescriptorSets(std::vector<RenderComponent>& pool);

	// Graphics Pipeline
	void CreateGraphicsPipeline();

	// Command Pool & Buffers
	void CreateCommandPool();
	void CreateCommandBuffers();
	void RecordCommandBuffer(uint32_t imageIndex);
	std::unique_ptr<vk::raii::CommandBuffer> BeginSingleTimeCommands();
	void EndSingleTimeCommands(const vk::raii::CommandBuffer& commandBuffer);

	// Texture
	void CreateTextureImage();
	void CreateTextureImageView();
	void CreateTextureSampler();

	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples,
		vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);
	[[nodiscard]] vk::raii::ImageView CreateImageView(const vk::raii::Image& image, vk::Format format,
		vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) const;
	void TransitionImageLayout(const vk::raii::Image& image, const vk::ImageLayout oldLayout,
		const vk::ImageLayout newLayout, uint32_t mipLevels);
	void TransitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
		vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
		vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
		vk::ImageAspectFlags aspectMask);
	void CopyBufferToImage(const vk::raii::Buffer& buffer, const vk::raii::Image& image, uint32_t width,
		uint32_t height);
	void GenerateMipmaps(const vk::raii::Image& image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight,
		uint32_t mipLevels);

	// Model
	void LoadModel();

	// Depth Buffer
	void CreateDepthResources();
	vk::Format FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
		vk::FormatFeatureFlags features) const;
	vk::Format FindDepthFormat() const;
	bool HasStencilComponent(vk::Format format) const;

	// Buffer Creation and Data Transfer
	void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void CreateUniformBuffers(std::vector<RenderComponent>& pool);
	uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
	void CopyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);

	// Synchronization
	void CreateSyncObjects();

	// Shader Loading
	std::vector<char> ReadFile(const std::string& filename);
	[[nodiscard]] vk::raii::ShaderModule CreateShaderModule(const std::vector<char>& code) const;

	// Multiple Sampling
	vk::SampleCountFlagBits GetMaxUsableSampleCount() const;
	void CreateColorResources();

	// Variables
	UtahCtx& _ctxRef;
	vk::raii::Context                _vkContext;
	vk::raii::Instance               _instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT _debugMessenger = nullptr;
	vk::raii::PhysicalDevice         _physicalDevice = nullptr;
	vk::raii::Device                 _device = nullptr;
	vk::raii::SurfaceKHR             _surface = nullptr;

	uint32_t        _queueIndex = ~0;
	vk::raii::Queue _queue = nullptr;

	vk::raii::SwapchainKHR           _swapChain = nullptr;
	std::vector<vk::Image>           _swapChainImages;
	vk::SurfaceFormatKHR             _swapChainSurfaceFormat;
	vk::Extent2D                     _swapChainExtent;
	std::vector<vk::raii::ImageView> _swapChainImageViews;

	vk::raii::DescriptorSetLayout _globalDescriptorSetLayout = nullptr;
	vk::raii::DescriptorSetLayout _objDescriptorSetLayout = nullptr;
	vk::raii::PipelineLayout      _pipelineLayout = nullptr;
	vk::raii::Pipeline            _graphicsPipeline = nullptr;

	vk::raii::Image        _colorImage = nullptr;
	vk::raii::DeviceMemory _colorImageMemory = nullptr;
	vk::raii::ImageView    _colorImageView = nullptr;

	vk::raii::Image        _depthImage = nullptr;
	vk::raii::DeviceMemory _depthImageMemory = nullptr;
	vk::raii::ImageView    _depthImageView = nullptr;

	uint32_t               _mipLevels = 0;
	vk::raii::Image        _textureImage = nullptr;
	vk::raii::DeviceMemory _textureImageMemory = nullptr;
	vk::raii::ImageView    _textureImageView = nullptr;
	vk::raii::Sampler      _textureSampler = nullptr;

	std::vector<Vertex>    _vertices;
	std::vector<uint32_t>  _indices;
	vk::raii::Buffer       _vertexBuffer = nullptr;
	vk::raii::DeviceMemory _vertexBufferMemory = nullptr;

	vk::raii::Buffer       _indexBuffer = nullptr;
	vk::raii::DeviceMemory _indexBufferMemory = nullptr;

	std::vector<vk::raii::Buffer>       _globalUniformBuffers;
	std::vector<vk::raii::DeviceMemory> _globalUniformBuffersMemory;
	std::vector<void*>                 _globalUniformBuffersMapped;

	vk::raii::DescriptorPool             _descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> _globalDescriptorSets;

	vk::raii::CommandPool                _commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> _commandBuffers;

	std::vector<vk::raii::Semaphore> _presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;
	std::vector<vk::raii::Fence>     _inFlightFences;

	uint32_t _semaphoreIndex = 0;
	uint32_t _currentFrame = 0;

	bool _framebufferResized = false;

	vk::SampleCountFlagBits _msaaSamples = vk::SampleCountFlagBits::e1;

	std::vector<const char*> _requiredDeviceExtension = { vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
														  vk::KHRSynchronization2ExtensionName,
														  vk::KHRCreateRenderpass2ExtensionName };

	std::vector<struct DrawJob> _drawList;

	struct CameraComponent _mainCam;

	std::vector<PointLightData> _pointLights;
};

