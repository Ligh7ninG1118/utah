#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>


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

	struct QueueFamilyIndices
	{
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

	//Variables
	GLFWwindow* _pWindow;

	VkInstance _instance;

	VkDebugUtilsMessengerEXT _debugMessenger;

	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;

	VkDevice _device;

	VkQueue _graphicsQueue;

	VkSurfaceKHR _surface;

	VkQueue _presentQueue;
};

