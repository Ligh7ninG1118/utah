#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <string>
#include <glm/glm.hpp>
#include <array>


struct Vertex
{
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Or can be Instance for instanced rendering
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
	{
		// For position and color
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		return attributeDescriptions;
	}
};

const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0,1,2,2,3,0
};


class AppCtx
{
public:
	AppCtx();
	~AppCtx();

	void Run();

private:
	//App Specific Functions
	void InitWindow();

	void InitVulkan();

	void MainLoop();

	void DrawFrame();

	void CleanUp();

	//Vulkan Specific Functions
	void CreateInstance();

	// Validation Layer & Debugging
	bool CheckValidationLayerSupprt();

	std::vector<const char*> GetRequiredExtensions();

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	
	void SetupDebugMessenger();

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	// Physical Device
	void PickPhysicalDevice();

	bool IsDeviceSuitable(VkPhysicalDevice device);

	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

	struct QueueFamilyIndices
	{
		// use optional wrapper: no value if not assigned anything
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool IsComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

	// Logical Device
	void CreateLogicalDevice();

	// Surface Creation
	void CreateSurface();

	// Swap Chain
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	// Resize Callback
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	void CreateSwapChain();

	// Image Views
	void CreateImageViews();

	// Graphics Pipeline
	void CreateGraphicsPipeline();

	// Render Pass
	void CreateRenderPass();

	// Framebuffers
	void CreateFramebuffers();

	// Command Pool & Buffers
	void CreateCommandPool();

	void CreateCommandBuffers();

	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	// Buffer Creation and Data Transfer
	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	void CreateVertexBuffer();

	void CreateIndexBuffer();

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	// Synchronization
	void CreateSyncObjects();

	// Swap Chain Recreation
	void CleanupSwapChain();

	void RecreateSwapChain();

	// Shader Loading
	std::vector<char> ReadFile(const std::string& filename);

	VkShaderModule CreateShaderModule(const std::vector<char>& code);

	//Variables
	GLFWwindow* _pWindow;

	VkInstance _instance;

	VkDebugUtilsMessengerEXT _debugMessenger;

	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;

	VkDevice _device;

	VkQueue _graphicsQueue;

	VkSurfaceKHR _surface;

	VkQueue _presentQueue;

	VkSwapchainKHR _swapChain;

	std::vector<VkImage> _swapChainImages;

	VkFormat _swapChainImageFormat;

	VkExtent2D _swapChainExtent;

	std::vector<VkImageView> _swapChainImageViews;

	VkRenderPass _renderPass;

	VkPipelineLayout _pipelineLayout;

	VkPipeline _graphicsPipeline;

	std::vector<VkFramebuffer> _swapChainFramebuffers;

	VkCommandPool _commandPool;

	std::vector<VkCommandBuffer> _commandBuffers;

	std::vector<VkSemaphore> _imageAvailableSemaphores;

	std::vector<VkSemaphore> _renderFinishedSemaphores;

	std::vector<VkFence> _inFlightFences;

	uint32_t _currentFrame = 0;

	bool _framebufferResized = false;

	VkBuffer _vertexBuffer;
	VkDeviceMemory _vertexBufferMemory;
	VkBuffer _indexBuffer;
	VkDeviceMemory _indexBufferMemory;
	
};

