#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <cassert>

#include "Registry.h"


class System;
class RenderSystem;

class UtahCtx
{
public:
	static UtahCtx& Get()
	{
		assert(_pInstance && "UtahCtx::Get() called before construction");
		return *_pInstance;
	}

	UtahCtx();
	~UtahCtx();

	void Run();

	GLFWwindow* GetContextWindow() const { return _pWindow; }

private:
	void InitWindow();
	void InitSystems();
	void InitDemo();
	void MainLoop();
	void CleanUp() const;

	static void MousePositionCallback(GLFWwindow* window, double xpos, double ypos);
	static void KeyInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

	static UtahCtx* _pInstance;

	GLFWwindow* _pWindow = nullptr;

	std::vector<System*> _systems;

	RenderSystem* _pRenderSystem;

	Registry _registry;

	double lastFrameTimestamp = 0.0;

};
