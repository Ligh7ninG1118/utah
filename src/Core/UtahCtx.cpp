#include "UtahCtx.h"
#include "Render/RenderSystem.h"
#include "Core/TransformComponent.h"
#include "Render/RenderComponent.h"
#include <utility>

constexpr uint32_t WINDOW_WIDTH = 1920;
constexpr uint32_t WINDOW_HEIGHT = 1080;

UtahCtx* UtahCtx::_instance = nullptr;


UtahCtx::UtahCtx()
{
	_instance = this;
}

UtahCtx::~UtahCtx()
{
	if (_instance == this)
		_instance = nullptr;
}

void UtahCtx::Run()
{
	InitWindow();
	InitSystems();
	InitDemo();
	MainLoop();
	CleanUp();
}

void UtahCtx::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	_pWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "utah", nullptr, nullptr);
	glfwSetWindowUserPointer(_pWindow, this);
}

void UtahCtx::InitSystems()
{
	RenderSystem* renderSys = new RenderSystem();
	_systems.push_back(renderSys);
	_renderSystem = renderSys;
}

void UtahCtx::InitDemo()
{
	for (int i = 0; i < 5; i++)
	{
		Entity* e = CreateEntity();

		TransformComponent transform{};
		transform._pos.x = i;
		AddComponent(transform, *e);

		RenderComponent render{};
		AddComponent(render, *e);
	}
}

void UtahCtx::MainLoop()
{
	while (!glfwWindowShouldClose(_pWindow))
	{
		glfwPollEvents();
		
		_renderSystem->Update(0.05f);
	}

	_renderSystem->WaitForRendererIdle();
}

void UtahCtx::CleanUp() const
{
	glfwDestroyWindow(_pWindow);

	glfwTerminate();

	//TODO: Destroy all entities, systems, components
}

Entity* UtahCtx::CreateEntity()
{
	static uint32_t lastID = 0;

	Entity* newEntity = new Entity(lastID++);

	_entities.insert(std::make_pair(newEntity->GetID(), newEntity));

	return newEntity;
}

void UtahCtx::AddComponent(const Component& comp, Entity& entity)
{
	uint8_t compID = GetComponentTypeID(comp);

	if (_componentsPool.size() <= compID)
		_componentsPool.resize(compID + 1);
	if (_components.size() <= compID)
		_components.resize(compID + 1);


	//Problem! This seems only store the vfptr of comp in memory?
	// Has to use template all over again...

	//TODO: Make sure this is tightly packed in memory
	_componentsPool[compID].push_back(comp); //memory contingiuous?
	//TODO: When deletion, do the swap with back trick
	_components[compID].push_back(_componentsPool[compID].size() - 1);
	//TODO: Figure out a way to add to entity (can't be pointer)(use the index trick again?)
}

uint8_t UtahCtx::GetComponentTypeID(const Component& comp)
{
	static uint8_t lastID = 0;

	auto type = std::type_index(typeid(comp));

	if (_componentTypeIDMap.contains(type))
		return _componentTypeIDMap.at(type);

	_componentTypeIDMap.insert(std::make_pair(type, lastID++));
	return lastID - 1;
}
