#include "UtahCtx.h"
#include "Render/RenderSystem.h"
#include "Gameplay/TransformComponent.h"
#include "Render/RenderComponent.h"
#include <utility>

constexpr uint32_t WINDOW_WIDTH = 1920;
constexpr uint32_t WINDOW_HEIGHT = 1080;

UtahCtx* UtahCtx::_pInstance = nullptr;


UtahCtx::UtahCtx()
	: _pRenderSystem(nullptr)
{
	_pInstance = this;
}

UtahCtx::~UtahCtx()
{
	if (_pInstance == this)
		_pInstance = nullptr;
}

void UtahCtx::Run()
{
	InitWindow();
	InitDemo();
	InitSystems();
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
	renderSys->Init(_registry);
	_pRenderSystem = renderSys;

	_systems.push_back(renderSys);
}

void UtahCtx::InitDemo()
{
	for (int i = 0; i < 5; i++)
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.x = -2 + i;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e);
	}
}

void UtahCtx::MainLoop()
{
	while (!glfwWindowShouldClose(_pWindow))
	{
		glfwPollEvents();
		
		auto& pool = _registry.GetPool<TransformComponent>()->GetPool();
		for (auto& obj : pool)
		{
			obj._rot.x += 0.00001f;
		}

		_pRenderSystem->Update(0.05f);

		_pRenderSystem->WaitForRendererIdle();
	}

}

void UtahCtx::CleanUp() const
{
	glfwDestroyWindow(_pWindow);

	glfwTerminate();

	//TODO: Destroy all entities, systems, components
}
