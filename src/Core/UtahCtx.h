#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <cassert>

#include "Entity.h"
#include "Component.h"

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


	Entity* CreateEntity();
	void AddComponent(const Component& comp, Entity& entity);
	uint8_t GetComponentTypeID(const Component& comp);

	static UtahCtx* _instance;

	GLFWwindow* _pWindow = nullptr;

	std::vector<System*> _systems;

	RenderSystem* _renderSystem;
	
	std::unordered_map<uint32_t, Entity*> _entities;

	std::unordered_map<std::type_index, uint8_t> _componentTypeIDMap;


	// [ComponentType][ComponentIndex]
	std::vector<std::vector<Component>> _componentsPool;
	// [ComponentType][IndexInPool]
	std::vector<std::vector<size_t>> _components;
};
