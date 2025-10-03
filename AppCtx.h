#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


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

	//Variables
	GLFWwindow* _pWindow;

	VkInstance _vkInstance;
};

