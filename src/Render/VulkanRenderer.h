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
#include "RenderCommons.h"


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


//TODO: temp solution to have different pipeline configuration, need a better one
enum class PipelineType
{
	Default = 0,			// Pos/Normal/UV, triangle list, fill
	Debug,					// Position only, triangle list, fill
	DebugWireframe,			// Position only, line list, no fill
};

enum class GlobalBinding : uint32_t
{
	CameraUBO = 0,
	LightUBO = 1,
	ObjectSSBO = 2,
	MaterialSSBO = 3,
	Textures = 4,
	TextureSamplers = 5,
	ShadowMapUBO = 6,
	ShadowMaps = 7,
	ShadowMapSampler = 8,
	ShadowCubeMaps = 9,
	HDROutput = 10,
	HDRSampler = 11,
	SkyboxCubemap = 12,
	Count
};

inline constexpr uint32_t ToIdx(GlobalBinding b) { return static_cast<uint32_t>(b); }

struct BindingDesc
{
	uint32_t bindingIndex;
	vk::DescriptorType type;
	uint32_t count;
	size_t bufferSize = 0; // Defaults to 0 for image/sampler
	vk::Flags<vk::ShaderStageFlagBits> stageFlags;
	vk::Flags<vk::DescriptorBindingFlagBits> bindingFlags;
};

// Everything that must exist once per frame in flight: resources the CPU writes or records
// while the GPU may still be consuming the previous frame's copy.
//
// Excludes:
//  - render-finished semaphores: per swapchain IMAGE (present consumes them), kept outside
//  - color/depth/shadow images: GPU-written persistents, synchronized by barriers
//  - swapchain images/views: count tied to the swapchain, not to frames in flight
struct FrameData
{
	// CPU-written, persistent-mapped buffers created from the BindingDesc table
	std::array<AllocatedBuffer, static_cast<size_t>(GlobalBinding::Count)> globalBuffers{};

	vk::raii::DescriptorSet globalDescriptorSet = nullptr;
	vk::raii::CommandBuffer commandBuffer = nullptr;

	vk::raii::Fence     inFlightFence = nullptr;
	vk::raii::Semaphore presentCompleteSemaphore = nullptr; // image-acquire semaphore

	AllocatedBuffer& Buffer(GlobalBinding b) { return globalBuffers[ToIdx(b)]; }
	[[nodiscard]] const AllocatedBuffer& Buffer(GlobalBinding b) const { return globalBuffers[ToIdx(b)]; }
	[[nodiscard]] void* Mapped(GlobalBinding b) const { return globalBuffers[ToIdx(b)].info.pMappedData; }
};

struct PipelineEntry
{
	vk::raii::Pipeline pipeline = nullptr;
	vk::PipelineLayout layout = nullptr;
};

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	void Initialize();

	void SetDrawList(std::vector<struct DrawJob>&& list);

	void SetDebugAABBDrawList(std::vector<glm::mat4>&& list);

	void SetCameraComponent(const CameraComponent& camera);

	void SetLights(std::vector<PointLightGPU>&& pointLights, std::vector<DirectionalLightGPU>&& dirLights, 
		std::vector<SpotLightGPU>&& spotLights);

	void DrawFrame();

	void WaitForIdle();

	void NotifyResized() { _framebufferResized = true; }

	//TODO: Sort these into a separate ResourceFactory class

	[[nodiscard]] AllocatedImage CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples,
		vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO, 
		VkImageCreateFlags creationFlags = 0, uint32_t arrayLayers = 1);
	[[nodiscard]] vk::raii::ImageView CreateImageView(vk::Image image, vk::Format format,
		vk::ImageAspectFlags aspectFlags, uint32_t mipLevels, vk::ImageViewType viewType = vk::ImageViewType::e2D, 
		uint32_t subresourceLayerCount = 1) const;

	void TransitionImageLayout(vk::Image image, vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount = 1);
	void TransitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
		vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
		vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
		vk::ImageAspectFlags aspectMask);

	void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height, uint32_t layerCount = 1);
	void GenerateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, uint32_t layerCount = 1);

	// Buffer Creation and Data Transfer
	[[nodiscard]] AllocatedBuffer CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
		VmaMemoryUsage memUsage, VmaAllocationCreateFlags allocFlags = 0);

	[[nodiscard]] AllocatedBuffer CreateDeviceLocalBuffer(const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage);

	void DestroyBuffer(AllocatedBuffer& buffer);
	void DestroyImage(AllocatedImage& image);
	void CreateUniformBuffers();
	void CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

	MeshManager* GetMeshManager() { return &_meshManager; }
	MaterialManager* GetMaterialManager() { return &_materialManager; }

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
	void InitBindingDescs();
	[[nodiscard]] vk::raii::DescriptorSetLayout CreateDescriptorSetLayout(const std::vector<BindingDesc>& descs);
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	void CreatePipelineLayouts();

	std::vector<BindingDesc> bindingDescs;

	// Graphics Pipeline
	uint32_t CreateGraphicsPipeline(const std::string& vertPath, const std::string& fragPath, 
		vk::PipelineLayout layout, PipelineType type = PipelineType::Default);
	uint32_t CreateShadowMapGraphicsPipeline(const std::string& vertPath, vk::PipelineLayout layout);
	uint32_t CreateShadowCubeMapGraphicsPipeline(const std::string& vertPath, vk::PipelineLayout layout);
	uint32_t CreateHDRGraphicsPipeline(const std::string& vertPath, const std::string& fragPath, vk::PipelineLayout layout);

	// Command Pool & Buffers
	void CreateCommandPool();
	void CreateCommandBuffers();
	//void RecordCommandBufferShadowMapView(uint32_t imageIndex);
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
	// Separate from the above function since no need to recreate
	void CreateHDRColorSampler();

	// Shadow Map
	void CreateShadowMapResources();

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
	vk::raii::PipelineLayout      _globalPipelineLayout = nullptr;
	std::vector<PipelineEntry> _pipelines;
	uint32_t _shadowPipelineIndex = INVALID_HANDLE;
	uint32_t _shadowCubeMapPipelineIndex = INVALID_HANDLE;
	uint32_t _hdrOutputPipelineIndex = INVALID_HANDLE;
	uint32_t _skyboxPipelineIndex = INVALID_HANDLE;
	MaterialHandle _debugAABBMaterial{};
	MaterialHandle _debugLightMaterial{};

	AllocatedImage		   _colorImage{};
	vk::raii::ImageView    _colorImageView = nullptr;

	AllocatedImage _hdrColorImage{};
	vk::raii::ImageView _hdrColorImageView = nullptr;
	vk::raii::Sampler _hdrSampler = nullptr;


	AllocatedImage		   _depthImage{};
	vk::raii::ImageView    _depthImageView = nullptr;

	uint32_t SHADOW_MAP_RESOLUTION = 4096;

	std::vector<AllocatedImage> _shadowMapImages;
	std::vector<vk::raii::ImageView> _shadowMapImageViews;
	vk::raii::Sampler _shadowMapSampler = nullptr;

	std::vector<AllocatedImage> _shadowCubeMapImages;
	std::vector<vk::raii::ImageView> _shadowCubeMapImageViews;

	TextureManager _textureManger;
	MeshManager _meshManager;
	MaterialManager _materialManager;

	vk::raii::DescriptorPool             _descriptorPool = nullptr;

	vk::raii::DescriptorPool			 _imguiDescriptorPool = nullptr;
	//Temp, drawing shadow map on imgui window
	VkDescriptorSet _shadowMapImGuiDS = VK_NULL_HANDLE;

	vk::raii::CommandPool                _commandPool = nullptr;
	// IMPORTANT: _frames is declared AFTER _descriptorPool and _commandPool on purpose.
	// Members destruct in reverse declaration order, so the raii descriptor set and
	// command buffer inside each FrameData are freed while their pools still exist.
	std::array<FrameData, MAX_FRAMES_IN_FLIGHT> _frames;

	// Waited on by vkQueuePresentKHR; tied to swapchain images, so per-IMAGE (not per-frame)
	std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;

	uint32_t _currentFrame = 0;

	bool _framebufferResized = false;

	vk::SampleCountFlagBits _msaaSamples = vk::SampleCountFlagBits::e1;

	std::vector<struct DrawJob> _drawList;
	std::vector<glm::mat4> _debugAABBDrawList;

	struct CameraComponent _mainCam;

	std::vector<PointLightGPU> _pointLights;
	std::vector<DirectionalLightGPU> _dirLights;
	std::vector<SpotLightGPU> _spotLights;
};