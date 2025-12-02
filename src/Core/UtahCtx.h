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


	static UtahCtx* _pInstance;

	GLFWwindow* _pWindow = nullptr;

	std::vector<System*> _systems;

	RenderSystem* _pRenderSystem;
	

	Registry _registry;
};
