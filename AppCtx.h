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
	void InitWindow();

	void InitVulkan();

	void MainLoop();

	void CleanUp();


	void CreateInstance();


	GLFWwindow* _pWindow;

	VkInstance _vkInstance;
};

