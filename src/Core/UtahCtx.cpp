#include "UtahCtx.h"
#include "Render/RenderSystem.h"
#include "Gameplay/TransformComponent.h"
#include "Render/RenderComponent.h"
#include <utility>
#include <iostream>

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

	//glfwSetInputMode(_pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(_pWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	glfwSetCursorPosCallback(_pWindow, MousePositionCallback);
	glfwSetKeyCallback(_pWindow, KeyInputCallback);
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
		
		double currentTimestamp = glfwGetTime();
		double deltaTime = currentTimestamp - lastFrameTimestamp;
		lastFrameTimestamp = currentTimestamp;

		auto& pool = _registry.GetPool<TransformComponent>()->GetPool();
		for (auto& obj : pool)
		{
			obj._rot.x += 0.00001f;
		}

		_pRenderSystem->Update(deltaTime);

		_pRenderSystem->WaitForRendererIdle();
	}

}

void UtahCtx::CleanUp() const
{
	glfwDestroyWindow(_pWindow);

	glfwTerminate();

	//TODO: Destroy all entities, systems, components
}

void UtahCtx::MousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	//std::cout << "xpos " << xpos << "\typos " << ypos << std::endl;
}

void UtahCtx::KeyInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_W && action == GLFW_PRESS)
		std::cout << "hello!" << std::endl;

	if(key == GLFW_KEY_W && action == GLFW_RELEASE)
		std::cout << "bye!" << std::endl;

	if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE)
	{
		UtahCtx* ctx = static_cast<UtahCtx*>(glfwGetWindowUserPointer(window));
		if (ctx)
			glfwSetWindowShouldClose(ctx->GetContextWindow(), GLFW_TRUE);
	}
}
