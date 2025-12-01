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
		assert(_instance && "UtahCtx::Get() called before construction");
		return *_instance;
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


	static UtahCtx* _instance;

	GLFWwindow* _pWindow = nullptr;

	std::vector<System*> _systems;

	RenderSystem* _renderSystem;
	

	Registry _registry;
};
