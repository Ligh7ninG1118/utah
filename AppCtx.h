#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>


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

	bool CheckValidationLayerSupprt();

	void PickPhysicalDevice();

	bool IsDeviceSuitable(VkPhysicalDevice device);

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

	//Variables
	GLFWwindow* _pWindow;

	VkInstance _vkInstance;

	VkDebugUtilsMessengerEXT _debugMessenger;
};

